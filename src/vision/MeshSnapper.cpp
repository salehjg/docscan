#include "vision/MeshSnapper.hpp"

#include "vision/Imaging.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

namespace docscan {

namespace {

constexpr int kWorkSize = 1000;       // long side of the flattened page the edges are searched on
constexpr double kNear = 0.03;        // how far a side may move to a paper edge, either way, as a fraction of the page
constexpr double kFar = 0.25;         // how far out it may move to the photo's border, which the page runs off
constexpr double kMinInk = 0.005;     // share of ink in what the outline left out, for it to be the page's
constexpr int kInkSize = 15;          // pixels: strokes thinner than this, darker than around them, are ink
constexpr int kInkContrast = 40;
constexpr double kBeyond = 0.03;      // how much of what lies beyond the paper's edge must stay darker
constexpr int kSamples = 64;          // points searched along each side
constexpr double kSampledSpan = 0.96; // of each side, centered: right at the corners the neighboring side interferes
constexpr float kMinStep = 10;        // whiteness the paper must have over what lies beyond its edge
constexpr int kStepWidth = 2;         // pixels either side of the edge: the step must be sharp, unlike a shadow
constexpr double kMinFound = 0.5;     // share of a side's points that must agree on its edge for the side to move
constexpr double kTolerance = 0.01;   // how far, as a fraction of the page, a point may lie off the side's curve
constexpr int kFitTrials = 200;
constexpr double kBeyondPercentile = 0.9;

enum Side { Top, Right, Bottom, Left };

/// Pixels across one side of the flattened page, from inside the page outward.
struct Profile {
    std::vector<float> whiteness;
    std::vector<bool> inPhoto; ///< false beyond the photo's border
    int near;                  ///< index of the farthest point within kNear outside the rough outline
    int beyond;                ///< pixels in kBeyond
    bool far;                  ///< whether the edge may be the photo's border further out than `near`
};

/// Index in the profile where the paper ends: the innermost sharp drop beyond which the next pixels stay
/// clearly darker (a dark bar printed near the edge has paper after it again). An edge further out than
/// `near` is taken only where it is the photo's own border, which the page runs off, and the profile allows it.
std::optional<int> paperEdge(const Profile& profile)
{
    const auto& w = profile.whiteness;
    const int n = int(w.size());
    const auto drop = [&](int i) { return w[i - kStepWidth] - w[i + kStepWidth]; };
    for (int i = kStepWidth; i + kStepWidth + 2 < n; ++i) {
        if (drop(i) < kMinStep)
            continue;
        const float paper = w[std::max(0, i - kStepWidth - 1)];
        const auto first = w.begin() + i + kStepWidth + 1;
        std::vector<float> beyond(first, first + std::min<std::ptrdiff_t>(profile.beyond, w.end() - first));
        const auto k = std::size_t(kBeyondPercentile * double(beyond.size() - 1));
        std::nth_element(beyond.begin(), beyond.begin() + std::ptrdiff_t(k), beyond.end());
        if (beyond[k] >= paper - kMinStep)
            continue;
        while (i + kStepWidth + 3 < n && drop(i + 1) > drop(i))
            ++i; // the steepest point of this step
        const bool atPhotoBorder = !profile.inPhoto[std::size_t(std::min(n - 1, i + kStepWidth + 1))];
        if (i <= profile.near || (profile.far && atPhotoBorder))
            return i;
        return std::nullopt; // a far edge: perhaps the table's or another sheet's, with the paper's too faint
    }
    return std::nullopt;
}

/// A side of the page: the page coordinate of the paper's edge as a quadratic in the position along it.
struct SideCurve {
    cv::Vec3d coefficients;

    double operator()(double along) const
    {
        const double t = std::clamp(along, 0.0, 1.0);
        return coefficients[0] + (coefficients[1] + coefficients[2] * t) * t;
    }
};

/// The quadratic through `points`: an exact one for three, a least-squares one for more.
std::optional<SideCurve> quadraticThrough(const std::vector<cv::Point2d>& points)
{
    cv::Mat a(int(points.size()), 3, CV_64F);
    cv::Mat b(int(points.size()), 1, CV_64F);
    for (int i = 0; i < a.rows; ++i) {
        a.at<double>(i, 0) = 1;
        a.at<double>(i, 1) = points[i].x;
        a.at<double>(i, 2) = points[i].x * points[i].x;
        b.at<double>(i) = points[i].y;
    }
    cv::Mat x;
    if (!cv::solve(a, b, x, points.size() == 3 ? cv::DECOMP_LU : cv::DECOMP_SVD))
        return std::nullopt;
    return SideCurve{{x.at<double>(0), x.at<double>(1), x.at<double>(2)}};
}

/// The curve most of the edge points `found` (position along the side, page coordinate of the edge) lie on:
/// RANSAC, then least squares over the points that agree. Shadows and printed bars give scattered wrong points.
/// Empty when fewer than kMinFound of the `samples` agree.
std::optional<SideCurve> fitSide(const std::vector<cv::Point2d>& found, int samples)
{
    const auto enough = [&](std::size_t n) { return double(n) >= kMinFound * samples; };
    if (!enough(found.size()))
        return std::nullopt;

    std::mt19937 rng(1); // fixed, so the same photo always gives the same page
    std::uniform_int_distribution<std::size_t> pick(0, found.size() - 1);
    std::vector<cv::Point2d> best;
    for (int trial = 0; trial < kFitTrials; ++trial) {
        const std::size_t i = pick(rng);
        const std::size_t j = pick(rng);
        const std::size_t k = pick(rng);
        if (i == j || j == k || i == k)
            continue;
        const auto curve = quadraticThrough({found[i], found[j], found[k]});
        if (!curve)
            continue;
        std::vector<cv::Point2d> agreeing;
        for (const auto& p : found) {
            if (std::abs((*curve)(p.x) - p.y) <= kTolerance)
                agreeing.push_back(p);
        }
        if (agreeing.size() > best.size())
            best = std::move(agreeing);
    }
    if (!enough(best.size()))
        return std::nullopt;
    return quadraticThrough(best);
}

} // namespace

PageMesh MeshSnapper::snap(const PageMesh& mesh, const cv::Mat& image) const
{
    // The page and kFar around it, flattened; outside the photo is black, so the photo's own edge counts as
    // the paper's edge where the page runs off it.
    const cv::Size2d pageSize = mesh.size();
    const double scale = kWorkSize / std::max(pageSize.width, pageSize.height);
    const cv::Size work(std::max(16, cvRound(pageSize.width * scale * (1 + 2 * kFar))),
                        std::max(16, cvRound(pageSize.height * scale * (1 + 2 * kFar))));
    const cv::Mat flat = mesh.unwarp(image, work, -kFar, cv::BORDER_CONSTANT);
    const cv::Mat white = eraseDetail(whiteness(flat));
    const cv::Mat everywhere(image.size(), CV_8UC1, cv::Scalar(255));
    const cv::Mat photo = mesh.unwarp(everywhere, work, -kFar, cv::BORDER_CONSTANT) > 127; // inside the photo

    // Ink: thin strokes darker than the paper around them, such as text and handwriting.
    cv::Mat ink;
    const cv::Mat strokeSize = cv::getStructuringElement(cv::MORPH_RECT, {kInkSize, kInkSize});
    cv::morphologyEx(toGray(flat), ink, cv::MORPH_BLACKHAT, strokeSize);
    ink = ink > kInkContrast;

    // Page coordinate <-> pixel of the flattened copy, across (x) and down (y).
    const auto toPixel = [&](double t, int pixels) { return (t + kFar) / (1 + 2 * kFar) * (pixels - 1); };
    const auto toPage = [&](double pixel, int pixels) { return pixel / (pixels - 1) * (1 + 2 * kFar) - kFar; };

    std::array<std::optional<SideCurve>, 4> sides;
    for (const Side side : {Top, Right, Bottom, Left}) {
        const bool horizontal = side == Top || side == Bottom;
        const int length = horizontal ? white.rows : white.cols; // pixels across the side
        const int along = horizontal ? white.cols : white.rows;
        const bool outwardIsUp = side == Top || side == Left;
        const double nominal = outwardIsUp ? 0.0 : 1.0;
        const int inner = cvRound(toPixel(outwardIsUp ? kNear : 1 - kNear, length));
        const int outer = outwardIsUp ? 0 : length - 1;
        const int step = outwardIsUp ? -1 : 1;
        const double pixelsPerPage = (length - 1) / (1 + 2 * kFar);

        // The outline may grow to the photo's border only over paper with ink on it: there the network has
        // missed part of a page that runs off the photo. Blank paper may be another sheet, or a white table.
        const int nearEdge = cvRound(toPixel(outwardIsUp ? -kNear : 1 + kNear, length));
        const cv::Range across = outwardIsUp ? cv::Range(0, std::max(1, nearEdge)) : cv::Range(nearEdge, length);
        const cv::Range lengthwise(cvRound(toPixel((1 - kSampledSpan) / 2, along)),
                                   cvRound(toPixel((1 + kSampledSpan) / 2, along)));
        const cv::Rect strip = horizontal ? cv::Rect(lengthwise.start, across.start, lengthwise.size(), across.size())
                                          : cv::Rect(across.start, lengthwise.start, across.size(), lengthwise.size());
        const int paper = cv::countNonZero(photo(strip));
        const bool far = paper > 0 && cv::countNonZero(ink(strip) & photo(strip)) >= kMinInk * paper;

        std::vector<cv::Point2d> found;
        for (int k = 0; k < kSamples; ++k) {
            const double position = (1 - kSampledSpan) / 2 + kSampledSpan * k / (kSamples - 1);
            const int p = std::clamp(cvRound(toPixel(position, along)), 0, along - 1);
            Profile profile{.whiteness = {},
                            .inPhoto = {},
                            .near = cvRound(2 * kNear * pixelsPerPage),
                            .beyond = std::max(2, cvRound(kBeyond * pixelsPerPage)),
                            .far = far};
            for (int q = inner; q != outer + step; q += step) {
                const cv::Point at = horizontal ? cv::Point(p, q) : cv::Point(q, p);
                profile.whiteness.push_back(white.at<uchar>(at));
                profile.inPhoto.push_back(photo.at<uchar>(at) != 0);
            }
            if (const auto i = paperEdge(profile))
                found.emplace_back(position, toPage(inner + step * *i, length));
        }
        sides[side] = fitSide(found, kSamples);
        if (!sides[side])
            sides[side] = SideCurve{{nominal, 0, 0}};
    }
    const SideCurve& top = *sides[Top];
    const SideCurve& right = *sides[Right];
    const SideCurve& bottom = *sides[Bottom];
    const SideCurve& left = *sides[Left];

    // Corners where neighboring sides meet, then a Coons patch between the four sides.
    const auto corner = [](const SideCurve& vertical, const SideCurve& horizontal, double u, double v) {
        for (int i = 0; i < 4; ++i) {
            u = vertical(v);
            v = horizontal(u);
        }
        return cv::Point2d(u, v);
    };
    const cv::Point2d c00 = corner(left, top, 0, 0);
    const cv::Point2d c10 = corner(right, top, 1, 0);
    const cv::Point2d c01 = corner(left, bottom, 0, 1);
    const cv::Point2d c11 = corner(right, bottom, 1, 1);

    cv::Mat points(mesh.points().size(), CV_32FC2);
    for (int r = 0; r < points.rows; ++r) {
        const double t = double(r) / (points.rows - 1);
        for (int c = 0; c < points.cols; ++c) {
            const double s = double(c) / (points.cols - 1);
            const double uTop = c00.x + (c10.x - c00.x) * s;
            const double uBottom = c01.x + (c11.x - c01.x) * s;
            const double vLeft = c00.y + (c01.y - c00.y) * t;
            const double vRight = c10.y + (c11.y - c10.y) * t;
            const cv::Point2d onTop(uTop, top(uTop));
            const cv::Point2d onBottom(uBottom, bottom(uBottom));
            const cv::Point2d onLeft(left(vLeft), vLeft);
            const cv::Point2d onRight(right(vRight), vRight);
            const cv::Point2d page = (1 - t) * onTop + t * onBottom + (1 - s) * onLeft + s * onRight
                                   - ((1 - s) * (1 - t) * c00 + s * (1 - t) * c10 + (1 - s) * t * c01 + s * t * c11);
            points.at<cv::Point2f>(r, c) = mesh.point(page.x, page.y);
        }
    }
    PageMesh snapped(points);
    return snapped.plausible(image.size()) ? snapped : mesh;
}

} // namespace docscan
