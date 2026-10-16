#include "vision/PageMesh.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace docscan {

namespace {

constexpr double kMinArea = 0.15;      // smallest page, as a fraction of the photo
constexpr float kBorderMargin = 0.02f; // fraction of the photo's long side

/// Cell index and position within it for the page coordinate `t` (0 to 1) on a line of `points` points;
/// beyond the ends the position runs past 0 or 1 in the outermost cell.
std::pair<int, float> cell(double t, int points)
{
    const double g = t * (points - 1);
    const int i = std::clamp(int(std::floor(g)), 0, points - 2);
    return {i, float(g - i)};
}

} // namespace

PageMesh::PageMesh(cv::Mat points) : points_(std::move(points))
{
    if (points_.type() != CV_32FC2 || points_.rows < 2 || points_.cols < 2)
        throw std::logic_error("PageMesh: expected at least 2 x 2 points of type CV_32FC2");
}

Quad PageMesh::outline() const
{
    const int last = points_.rows - 1;
    const int right = points_.cols - 1;
    return Quad({at(0, 0), at(0, right), at(last, right), at(last, 0)});
}

cv::Size2d PageMesh::size() const
{
    double across = 0;
    double down = 0;
    for (int r = 0; r < points_.rows; ++r) {
        for (int c = 0; c + 1 < points_.cols; ++c)
            across += cv::norm(at(r, c + 1) - at(r, c));
    }
    for (int c = 0; c < points_.cols; ++c) {
        for (int r = 0; r + 1 < points_.rows; ++r)
            down += cv::norm(at(r + 1, c) - at(r, c));
    }
    return {across / points_.rows, down / points_.cols};
}

bool PageMesh::touchesBorder(cv::Size imageSize) const
{
    const float margin = kBorderMargin * float(std::max(imageSize.width, imageSize.height));
    const auto near = [&](cv::Point2f p) {
        return p.x < margin || p.y < margin || p.x > float(imageSize.width - 1) - margin
            || p.y > float(imageSize.height - 1) - margin;
    };
    for (int r = 0; r < points_.rows; ++r) {
        if (near(at(r, 0)) || near(at(r, points_.cols - 1)))
            return true;
    }
    for (int c = 0; c < points_.cols; ++c) {
        if (near(at(0, c)) || near(at(points_.rows - 1, c)))
            return true;
    }
    return false;
}

bool PageMesh::plausible(cv::Size imageSize) const
{
    // Every cell keeps the orientation of the page: across to the right, down below (clockwise on screen).
    for (int r = 0; r + 1 < points_.rows; ++r) {
        for (int c = 0; c + 1 < points_.cols; ++c) {
            if ((at(r, c + 1) - at(r, c)).cross(at(r + 1, c) - at(r, c)) <= 0)
                return false;
        }
    }
    std::vector<cv::Point2f> boundary;
    for (int c = 0; c < points_.cols; ++c)
        boundary.push_back(at(0, c));
    for (int r = 1; r < points_.rows; ++r)
        boundary.push_back(at(r, points_.cols - 1));
    for (int c = points_.cols - 2; c >= 0; --c)
        boundary.push_back(at(points_.rows - 1, c));
    for (int r = points_.rows - 2; r > 0; --r)
        boundary.push_back(at(r, 0));
    return cv::contourArea(boundary) >= kMinArea * double(imageSize.area());
}

cv::Point2f PageMesh::point(double u, double v) const
{
    const auto [r, fy] = cell(v, points_.rows);
    const auto [c, fx] = cell(u, points_.cols);
    const cv::Point2f upper = at(r, c) * (1 - fx) + at(r, c + 1) * fx;
    const cv::Point2f lower = at(r + 1, c) * (1 - fx) + at(r + 1, c + 1) * fx;
    return upper * (1 - fy) + lower * fy;
}

cv::Mat PageMesh::unwarp(const cv::Mat& image, cv::Size size, double trim, int border) const
{
    // Bilinear interpolation between the mesh points, which sit at even steps across and down the page.
    const auto position = [&](int pixel, int pixels, int points) {
        const double t = pixels > 1 ? double(pixel) / (pixels - 1) : 0.5;
        return cell(trim + (1 - 2 * trim) * t, points);
    };

    cv::Mat mapX(size, CV_32F);
    cv::Mat mapY(size, CV_32F);
    std::vector<cv::Point2f> line(std::size_t(points_.cols));
    for (int y = 0; y < size.height; ++y) {
        const auto [r, fy] = position(y, size.height, points_.rows);
        for (int c = 0; c < points_.cols; ++c)
            line[c] = at(r, c) * (1 - fy) + at(r + 1, c) * fy;
        auto* xs = mapX.ptr<float>(y);
        auto* ys = mapY.ptr<float>(y);
        for (int x = 0; x < size.width; ++x) {
            const auto [c, fx] = position(x, size.width, points_.cols);
            const cv::Point2f p = line[c] * (1 - fx) + line[c + 1] * fx;
            xs[x] = p.x;
            ys[x] = p.y;
        }
    }
    cv::Mat flat;
    cv::remap(image, flat, mapX, mapY, cv::INTER_CUBIC, border);
    return flat;
}

} // namespace docscan
