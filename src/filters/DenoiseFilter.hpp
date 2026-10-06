#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Non-local means denoising: removes grain from dark, noisy photos.
class DenoiseFilter final : public Filter {
public:
    static FilterInfo info();
    explicit DenoiseFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    float strength_;
};

} // namespace docscan
