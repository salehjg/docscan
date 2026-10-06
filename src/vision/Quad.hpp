#pragma once

#include <opencv2/core.hpp>

#include <array>
#include <cstddef>

namespace docscan {

/// The four corners of a page in a photo, ordered top-left, top-right, bottom-right, bottom-left.
class Quad {
public:
    /// Accepts the corners in any order.
    explicit Quad(const std::array<cv::Point2f, 4>& corners);

    const std::array<cv::Point2f, 4>& corners() const { return corners_; }

    double area() const;
    double width() const;  ///< longer of the top and bottom edges
    double height() const; ///< longer of the left and right edges
    Quad scaled(double factor) const;

    /// True when side `index` (0 top, 1 right, 2 bottom, 3 left) runs along the border of an image of
    /// `imageSize`: there the page goes on beyond the photo.
    bool sideOnBorder(std::size_t index, cv::Size imageSize) const;
    /// True when any side runs along the photo border, so the page is not fully in view.
    bool touchesBorder(cv::Size imageSize) const;

    /// Height / width of the real rectangle, undoing the perspective of a camera centered on `imageSize`
    /// (Zhang & He, "Whiteboard scanning and image enhancement", 2007). When the focal length cannot be
    /// recovered from the corners, a typical phone camera is assumed.
    double trueAspect(cv::Size imageSize) const;

private:
    std::array<cv::Point2f, 4> corners_;
};

} // namespace docscan
