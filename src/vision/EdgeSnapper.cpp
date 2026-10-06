#include "vision/EdgeSnapper.hpp"

#include "vision/Imaging.hpp"

#include <opencv2/geometry.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace docscan {

namespace {

constexpr int kWorkSize = 1600;                  // long side of the copy the edges are searched on
constexpr std::array kSearchBands{0.05f, 0.015f}; // how far sides may move per pass, as a fraction of the long side
constexpr float kMinStep = 8;                    // weaker steps are not taken for the paper's edge
constexpr float kEdgeShare = 0.4f;               // the paper's edge is the first step this close to the strongest
constexpr int kSamplesPerSide = 50;

/// An infinite line through `point` along the unit vector `direction`.
struct Line {
    cv::Point2f point;
    cv::Point2f direction;
};

std::optional<cv::Point2f> intersection(const Line& a, const Line& b)
{
    const float cross = a.direction.cross(b.direction);
    if (std::abs(cross) < 1e-6f)
        return std::nullopt;
    return a.point + a.direction * ((b.point - a.point).cross(b.direction) / cross);
}

/// Bilinear sample of an 8-bit image; empty outside it.
std::optional<float> sample(const cv::Mat& image, cv::Point2f p)
{
    if (p.x < 0 || p.y < 0 || p.x > float(image.cols - 2) || p.y > float(image.rows - 2))
        return std::nullopt;
    const int x = int(p.x);
    const int y = int(p.y);
    const float fx = p.x - float(x);
    const float fy = p.y - float(y);
    const auto at = [&](int dx, int dy) { return float(image.at<uchar>(y + dy, x + dx)); };
    return (at(0, 0) * (1 - fx) + at(1, 0) * fx) * (1 - fy) + (at(0, 1) * (1 - fx) + at(1, 1) * fx) * fy;
}

/// Offset along `outward` from `base` where the paper ends. Scanning outward, that is the first clear
/// drop in whiteness: further out there may be stronger ones, such as the edge of the table itself.
std::optional<float> paperEdge(const cv::Mat& white, cv::Point2f base, cv::Point2f outward, float band)
{
    std::vector<float> offsets;
    std::vector<float> drops;
    for (float offset = -band; offset <= band; offset += 1) {
        const auto inside = sample(white, base + outward * (offset - 2));
        const auto outside = sample(white, base + outward * (offset + 2));
        if (inside && outside) {
            offsets.push_back(offset);
            drops.push_back(*inside - *outside);
        }
    }
    if (drops.empty() || std::ranges::max(drops) < kMinStep)
        return std::nullopt;

    const float clear = std::max(kMinStep, kEdgeShare * std::ranges::max(drops));
    auto k = std::size_t(std::ranges::find_if(drops, [&](float drop) { return drop >= clear; }) - drops.begin());
    while (k + 1 < drops.size() && drops[k + 1] > drops[k])
        ++k; // climb to the steepest point of this edge
    return offsets[k];
}

/// The side from `a` to `b` moved (by up to `band` pixels) onto the paper's edge,
/// or left as it is when no clear edge runs along it.
Line snapSide(cv::Point2f a, cv::Point2f b, const cv::Mat& white, float band)
{
    const cv::Point2f along = (b - a) / float(cv::norm(b - a));
    const cv::Point2f outward(along.y, -along.x); // corners run clockwise on screen

    std::vector<cv::Point2f> edge;
    for (int s = 0; s <= kSamplesPerSide; ++s) {
        // Stay away from the corners, where the neighboring sides interfere.
        const cv::Point2f base = a + (b - a) * (0.1f + 0.8f * float(s) / kSamplesPerSide);
        if (const auto offset = paperEdge(white, base, outward, band))
            edge.push_back(base + outward * *offset);
    }
    if (edge.size() < kSamplesPerSide / 2)
        return {a, along};

    cv::Vec4f fit;
    cv::fitLine(edge, fit, cv::DIST_HUBER, 0, 0.01, 0.01);
    return {{fit[2], fit[3]}, {fit[0], fit[1]}};
}

/// One pass over all sides. Gives up (returns `quad`) if a corner would move further than the search allows.
Quad snapPass(const Quad& quad, const cv::Mat& white, float band)
{
    const auto& c = quad.corners();
    std::array<Line, 4> sides;
    for (std::size_t i = 0; i < 4; ++i) {
        const cv::Point2f a = c[i];
        const cv::Point2f b = c[(i + 1) % 4];
        sides[i] = quad.sideOnBorder(i, white.size()) ? Line{a, (b - a) / float(cv::norm(b - a))}
                                                      : snapSide(a, b, white, band);
    }

    // Corner i is where side i-1 ends and side i starts.
    std::array<cv::Point2f, 4> corners;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto corner = intersection(sides[(i + 3) % 4], sides[i]);
        if (!corner || cv::norm(*corner - c[i]) > 2 * band)
            return quad;
        corners[i] = *corner;
    }
    return Quad(corners);
}

} // namespace

Quad EdgeSnapper::snap(const Quad& rough, const cv::Mat& image) const
{
    const double factor = shrinkFactor(image.size(), kWorkSize);
    const cv::Mat white = eraseDetail(whiteness(shrink(image, factor)));
    const float longSide = float(std::max(white.cols, white.rows));

    Quad quad = rough.scaled(factor);
    for (const float band : kSearchBands)
        quad = snapPass(quad, white, band * longSide);
    return quad.scaled(1.0 / factor);
}

} // namespace docscan
