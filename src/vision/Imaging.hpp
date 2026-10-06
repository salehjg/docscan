#pragma once

#include <opencv2/core.hpp>

namespace docscan {

/// The image as 8-bit grayscale (returned as is when it already has one channel).
cv::Mat toGray(const cv::Mat& image);

/// Factor that shrinks `size` so its longest side is at most `longSide` (1 when it already fits).
double shrinkFactor(cv::Size size, int longSide);

/// A copy shrunk by `factor` (area interpolation); the image itself when factor is 1.
cv::Mat shrink(const cv::Mat& image, double factor);

/// Brightness minus saturation: high on paper, lower on most tables and backgrounds.
cv::Mat whiteness(const cv::Mat& image);

/// A single-channel image with text and fine detail erased, so only large shapes such as a page remain.
cv::Mat eraseDetail(const cv::Mat& image);

} // namespace docscan
