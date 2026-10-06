#include "vision/Quad.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace docscan;
using Catch::Matchers::WithinRel;

namespace {

const cv::Size kImage(3000, 4000);

cv::Matx33d rotationX(double degrees)
{
    const double a = degrees * std::numbers::pi / 180;
    return {1, 0, 0, 0, std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a)};
}

cv::Matx33d rotationY(double degrees)
{
    const double a = degrees * std::numbers::pi / 180;
    return {std::cos(a), 0, std::sin(a), 0, 1, 0, -std::sin(a), 0, std::cos(a)};
}

/// Photographs an A4 page (in mm) with a pinhole camera of focal length `focal` pixels.
Quad photographA4(const cv::Matx33d& rotation, double focal)
{
    std::array<cv::Point2f, 4> corners;
    const cv::Vec3d page[] = {{-105, -148.5, 0}, {105, -148.5, 0}, {105, 148.5, 0}, {-105, 148.5, 0}};
    for (std::size_t i = 0; i < 4; ++i) {
        const cv::Vec3d p = rotation * page[i] + cv::Vec3d(10, -20, 500);
        corners[i] = cv::Point2f(float(focal * p[0] / p[2] + kImage.width / 2.0),
                                 float(focal * p[1] / p[2] + kImage.height / 2.0));
    }
    return Quad(corners);
}

} // namespace

TEST_CASE("corners are ordered top-left, top-right, bottom-right, bottom-left")
{
    const Quad quad({cv::Point2f(90, 110), {10, 100}, {100, 10}, {5, 5}});
    const auto& c = quad.corners();
    CHECK(c[0] == cv::Point2f(5, 5));
    CHECK(c[1] == cv::Point2f(100, 10));
    CHECK(c[2] == cv::Point2f(90, 110));
    CHECK(c[3] == cv::Point2f(10, 100));
}

TEST_CASE("the true shape of a page photographed at an angle is recovered")
{
    const Quad quad = photographA4(rotationX(30) * rotationY(20), 2600);
    CHECK_THAT(quad.trueAspect(kImage), WithinRel(std::numbers::sqrt2, 0.01));
    CHECK(std::abs(quad.height() / quad.width() - std::numbers::sqrt2) > 0.05); // the photo alone misleads
}

TEST_CASE("a tilt about one axis is undone with a typical focal length")
{
    const double typicalFocal = 0.6 * std::hypot(kImage.width, kImage.height);
    const Quad quad = photographA4(rotationX(35), typicalFocal);
    CHECK_THAT(quad.trueAspect(kImage), WithinRel(std::numbers::sqrt2, 0.01));
}

TEST_CASE("a page running off the photo touches its border")
{
    const cv::Size photo(1000, 1000);
    const Quad inside({cv::Point2f(100, 100), {900, 120}, {880, 900}, {120, 880}});
    const Quad cutOff({cv::Point2f(100, 0), {900, 0}, {880, 900}, {120, 880}});
    CHECK_FALSE(inside.touchesBorder(photo));
    CHECK(cutOff.touchesBorder(photo));
    CHECK(cutOff.sideOnBorder(0, photo));
    CHECK_FALSE(cutOff.sideOnBorder(1, photo));
}
