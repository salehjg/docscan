#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Turns the page clockwise. Quarter turns are lossless; other angles grow the canvas with white.
class RotateFilter final : public Filter {
public:
    static FilterInfo info();
    explicit RotateFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    double angle_; ///< degrees clockwise, normalized to [0, 360)
};

} // namespace docscan
