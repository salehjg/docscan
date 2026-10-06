#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Evens out uneven lighting: estimates the paper's brightness everywhere on the page
/// and divides it out, which removes shadows and turns the paper white.
class LightFilter final : public Filter {
public:
    static FilterInfo info();
    explicit LightFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    double strength_;
};

} // namespace docscan
