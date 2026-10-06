#include "vision/Quad.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace docscan {

namespace {

constexpr double kTypicalFocalLength = 0.6; // phone main camera (~26 mm equivalent), in image diagonals
constexpr double kMinFocalLength = 0.25;
constexpr double kMaxFocalLength = 3.0;
constexpr float kBorderMargin = 0.005f; // fraction of the image's long side

double distance(cv::Point2f a, cv::Point2f b) { return cv::norm(a - b); }

} // namespace

Quad::Quad(const std::array<cv::Point2f, 4>& corners) : corners_(corners)
{
    // Sorting by angle around the center gives clockwise order on screen (y points down);
    // then rotate so the corner nearest the image origin comes first.
    const cv::Point2f center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
    std::ranges::sort(corners_, {}, [&](cv::Point2f p) { return std::atan2(p.y - center.y, p.x - center.x); });
    const auto topLeft = std::ranges::min_element(corners_, {}, [](cv::Point2f p) { return p.x + p.y; });
    std::ranges::rotate(corners_, topLeft);
}

double Quad::area() const
{
    return cv::contourArea(std::vector<cv::Point2f>(corners_.begin(), corners_.end()));
}

double Quad::width() const
{
    return std::max(distance(corners_[0], corners_[1]), distance(corners_[3], corners_[2]));
}

double Quad::height() const
{
    return std::max(distance(corners_[0], corners_[3]), distance(corners_[1], corners_[2]));
}

Quad Quad::scaled(double factor) const
{
    auto corners = corners_;
    for (auto& corner : corners)
        corner *= float(factor);
    return Quad(corners);
}

bool Quad::sideOnBorder(std::size_t index, cv::Size imageSize) const
{
    const cv::Point2f a = corners_[index];
    const cv::Point2f b = corners_[(index + 1) % 4];
    const float margin = kBorderMargin * float(std::max(imageSize.width, imageSize.height));
    const auto near = [&](float value, float edge) { return std::abs(value - edge) <= margin; };
    const float right = float(imageSize.width - 1);
    const float bottom = float(imageSize.height - 1);
    return (near(a.x, 0) && near(b.x, 0)) || (near(a.y, 0) && near(b.y, 0)) || (near(a.x, right) && near(b.x, right))
        || (near(a.y, bottom) && near(b.y, bottom));
}

bool Quad::touchesBorder(cv::Size imageSize) const
{
    for (std::size_t i = 0; i < 4; ++i) {
        if (sideOnBorder(i, imageSize))
            return true;
    }
    return false;
}

double Quad::trueAspect(cv::Size imageSize) const
{
    // Homogeneous corners relative to the principal point, named as in the paper:
    // m1 top-left, m2 top-right, m3 bottom-left, m4 bottom-right.
    const cv::Point2d center(imageSize.width / 2.0, imageSize.height / 2.0);
    const auto homogeneous = [&](cv::Point2f p) { return cv::Vec3d(p.x - center.x, p.y - center.y, 1.0); };
    const cv::Vec3d m1 = homogeneous(corners_[0]);
    const cv::Vec3d m2 = homogeneous(corners_[1]);
    const cv::Vec3d m3 = homogeneous(corners_[3]);
    const cv::Vec3d m4 = homogeneous(corners_[2]);

    const double k2 = m1.cross(m4).dot(m3) / m2.cross(m4).dot(m3);
    const double k3 = m1.cross(m4).dot(m2) / m3.cross(m4).dot(m2);
    const cv::Vec3d n2 = k2 * m2 - m1; // direction of the top edge in space
    const cv::Vec3d n3 = k3 * m3 - m1; // direction of the left edge in space

    const double diagonal = std::hypot(imageSize.width, imageSize.height);
    double focalSquared = std::pow(kTypicalFocalLength * diagonal, 2);
    if (const double denominator = n2[2] * n3[2]; denominator != 0) {
        const double estimate = -(n2[0] * n3[0] + n2[1] * n3[1]) / denominator;
        if (estimate > std::pow(kMinFocalLength * diagonal, 2) && estimate < std::pow(kMaxFocalLength * diagonal, 2))
            focalSquared = estimate;
    }

    const double widthSquared = (n2[0] * n2[0] + n2[1] * n2[1]) / focalSquared + n2[2] * n2[2];
    const double heightSquared = (n3[0] * n3[0] + n3[1] * n3[1]) / focalSquared + n3[2] * n3[2];
    const double aspect = std::sqrt(heightSquared / widthSquared);
    return std::isfinite(aspect) && aspect > 0 ? aspect : height() / width();
}

} // namespace docscan
