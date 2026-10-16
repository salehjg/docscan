#pragma once

#include "vision/Quad.hpp"

#include <opencv2/core.hpp>

namespace docscan {

/// Where a regular grid laid over the flat page lies in the photo: rows of points running down the page,
/// each row running across it. Unlike a Quad it follows a page that is curled, bent or folded.
class PageMesh {
public:
    /// `points` is CV_32FC2 with at least 2 x 2 points, in the photo's pixel coordinates.
    explicit PageMesh(cv::Mat points);

    const cv::Mat& points() const { return points_; }

    /// The mesh's corners, as an outline of the page.
    Quad outline() const;
    /// Mean length in pixels of the lines across the page (width) and down it (height), along its curves.
    cv::Size2d size() const;
    /// True when the mesh comes close to the border of an image of `imageSize`: the page goes on beyond it.
    bool touchesBorder(cv::Size imageSize) const;
    /// True when the mesh can be a page: no cell folded over, and a large enough part of the photo.
    bool plausible(cv::Size imageSize) const;

    /// Where the page point (u, v) lies in the photo: (0, 0) is the page's top-left corner, (1, 1) its
    /// bottom-right one. Outside that square the mesh is extended along its outermost cells.
    cv::Point2f point(double u, double v) const;

    /// The page flattened out of `image` onto a `size` rectangle, leaving out `trim` (a fraction) of each edge;
    /// a negative `trim` takes in that much around the page instead. `border` fills what lies outside the photo.
    cv::Mat unwarp(const cv::Mat& image, cv::Size size, double trim, int border = cv::BORDER_REPLICATE) const;

private:
    cv::Point2f at(int row, int column) const { return points_.at<cv::Point2f>(row, column); }

    cv::Mat points_;
};

} // namespace docscan
