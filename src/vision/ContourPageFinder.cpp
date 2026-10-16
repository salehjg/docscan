#include "vision/ContourPageFinder.hpp"

#include "vision/Imaging.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace docscan {

namespace {

constexpr int kWorkSize = 800;         // long side of the copy the page is searched on
constexpr double kMinArea = 0.15;      // smallest page, as a fraction of the photo
constexpr double kMinCornerAngle = 45; // degrees; perspective rarely bends a corner further
constexpr double kMaxCornerAngle = 135;
constexpr int kSamplesPerSide = 50;
constexpr float kPaperReach = 0.015f; // how far inside and outside a side paper and background are compared,
                                      // as a fraction of the long side
constexpr float kMinPaperStep = 10;   // whiteness the paper must have over what lies just outside it, lost
                                      // within a few pixels at its edge (shadows fade out slowly)
constexpr double kMinPaperEdge = 0.5; // share of every side that must look like the edge of the paper

using Contour = std::vector<cv::Point>;

cv::Mat structuringElement(int size) { return cv::getStructuringElement(cv::MORPH_RECT, {size, size}); }

/// Outlines of everything that differs from its surroundings, the page border included.
cv::Mat edgeMap(const cv::Mat& plain)
{
    cv::Mat edges;
    cv::Canny(plain, edges, 20, 60);
    cv::dilate(edges, edges, structuringElement(3));
    return edges;
}

/// Bright, colorless pixels: paper, as opposed to most tables and backgrounds.
cv::Mat paperMask(const cv::Mat& image)
{
    cv::Mat white;
    cv::GaussianBlur(whiteness(image), white, {7, 7}, 0);
    cv::Mat mask;
    cv::threshold(white, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, structuringElement(7));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, structuringElement(7));
    return mask;
}

/// Four corners that best outline `contour`.
Quad quadAround(const Contour& contour)
{
    Contour hull;
    cv::convexHull(contour, hull);
    const double perimeter = cv::arcLength(hull, true);
    Contour polygon;
    for (double tolerance = 0.01; tolerance <= 0.1; tolerance += 0.005) {
        cv::approxPolyDP(hull, polygon, tolerance * perimeter, true);
        if (polygon.size() == 4)
            return Quad({polygon[0], polygon[1], polygon[2], polygon[3]});
        if (polygon.size() < 4)
            break;
    }
    std::array<cv::Point2f, 4> box;
    cv::minAreaRect(hull).points(box.data());
    return Quad(box);
}

bool hasPlausibleCorners(const Quad& quad)
{
    const auto& c = quad.corners();
    for (std::size_t i = 0; i < 4; ++i) {
        const cv::Point2f toPrevious = c[(i + 3) % 4] - c[i];
        const cv::Point2f toNext = c[(i + 1) % 4] - c[i];
        const double cosine = toPrevious.dot(toNext) / (cv::norm(toPrevious) * cv::norm(toNext));
        const double degrees = std::acos(std::clamp(cosine, -1.0, 1.0)) * 180.0 / std::numbers::pi;
        if (!(degrees >= kMinCornerAngle && degrees <= kMaxCornerAngle))
            return false;
    }
    return true;
}

/// Whether `white` steps down sharply from paper to a darker outside near `p`, looking up to `reach` pixels
/// either way along `outward`; both ends of that search must lie in the image.
bool paperEndsNear(const cv::Mat& white, cv::Point2f p, cv::Point2f outward, float reach)
{
    const auto at = [&](float offset) { return float(white.at<uchar>(cv::Point(p + outward * offset))); };
    if (at(-reach) < at(reach) + kMinPaperStep)
        return false;
    for (float offset = 2 - reach; offset <= reach - 2; offset += 1) {
        if (at(offset - 2) >= at(offset + 2) + kMinPaperStep)
            return true;
    }
    return false;
}

/// How well a candidate page outline follows the real edges of the paper.
struct SideEvidence {
    double edgeFit;   ///< share of the points along the sides that lie on detected edges
    double paperEdge; ///< share of the weakest side that runs along an edge or where whiter paper meets a darker outside
};

/// Evidence from the page's sides that are not on the photo border.
/// Empty when every side lies on the border: then the "page" is just the photo itself.
std::optional<SideEvidence> sideEvidence(const Quad& quad, const cv::Mat& edges, const cv::Mat& white)
{
    const cv::Rect bounds({}, edges.size());
    const float reach = kPaperReach * float(std::max(edges.cols, edges.rows));
    int samples = 0;
    int hits = 0;
    double weakest = 1;
    const auto& c = quad.corners();
    for (std::size_t i = 0; i < 4; ++i) {
        if (quad.sideOnBorder(i, edges.size()))
            continue;
        const cv::Point2f a = c[i];
        const cv::Point2f b = c[(i + 1) % 4];
        const cv::Point2f along = (b - a) / float(cv::norm(b - a));
        const cv::Point2f outward(along.y, -along.x); // corners run clockwise on screen
        int compared = 0;
        int steps = 0;
        for (int s = 0; s <= kSamplesPerSide; ++s) {
            const cv::Point2f p = a + (b - a) * (float(s) / kSamplesPerSide);
            const bool onEdge = bounds.contains(p) && edges.at<uchar>(p) != 0;
            ++samples;
            hits += onEdge;

            // A line drawn across the page, through text, tables or shadows, has paper on both sides and few edges.
            if (!bounds.contains(cv::Point(p - outward * reach)) || !bounds.contains(cv::Point(p + outward * reach)))
                continue; // the side runs along the photo border here
            ++compared;
            steps += onEdge || paperEndsNear(white, p, outward, reach);
        }
        if (compared > kSamplesPerSide / 2)
            weakest = std::min(weakest, double(steps) / compared);
    }
    if (samples == 0)
        return std::nullopt;
    return SideEvidence{double(hits) / samples, weakest};
}

} // namespace

std::optional<Quad> ContourPageFinder::find(const cv::Mat& image) const
{
    const double factor = shrinkFactor(image.size(), kWorkSize);
    const cv::Mat small = shrink(image, factor);
    const double photoArea = double(small.total());

    cv::Mat edges = edgeMap(eraseDetail(toGray(small)));
    const cv::Mat white = eraseDetail(whiteness(small));
    cv::Mat support;
    cv::dilate(edges, support, structuringElement(5));

    // Closing the edge map along the photo border turns pages cut off by the frame into closed shapes.
    cv::rectangle(edges, cv::Rect({}, edges.size()), 255, 1);
    std::vector<Contour> contours;
    cv::findContours(edges, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    std::vector<Contour> paperContours;
    cv::findContours(paperMask(small), paperContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    contours.insert(contours.end(), paperContours.begin(), paperContours.end());

    std::optional<Quad> best;
    double bestScore = 0;
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) < kMinArea * photoArea)
            continue;
        const Quad quad = quadAround(contour);
        const double area = quad.area() / photoArea;
        if (area < kMinArea || !hasPlausibleCorners(quad))
            continue;
        const auto evidence = sideEvidence(quad, support, white);
        if (!evidence || evidence->paperEdge < kMinPaperEdge)
            continue;
        const double score = area * (0.3 + 0.7 * evidence->edgeFit);
        if (score > bestScore) {
            best = quad;
            bestScore = score;
        }
    }
    if (!best)
        return std::nullopt;
    return snapper_.snap(best->scaled(1.0 / factor), image);
}

} // namespace docscan
