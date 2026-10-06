#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Converts the page to grayscale.
class GrayFilter final : public Filter {
public:
    static FilterInfo info();
    explicit GrayFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;
};

} // namespace docscan
