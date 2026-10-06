#pragma once

#include "vision/Quad.hpp"

#include <opencv2/core.hpp>

namespace docscan {

/// Moves the sides of a rough page outline onto the paper's real edges, for a tight crop.
/// Each side is searched across for the step from white paper to the darker or more colorful
/// background, a line is fitted through those points, and neighboring lines meet in the new corners.
/// Sides on the photo border stay where they are: there the page goes on beyond the photo.
class EdgeSnapper {
public:
    /// `rough` is given in the pixel coordinates of `image`, and so is the result.
    [[nodiscard]] Quad snap(const Quad& rough, const cv::Mat& image) const;
};

} // namespace docscan
