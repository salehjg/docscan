#pragma once

#include "filters/Filter.hpp"
#include "vision/Binarizer.hpp"

#include <memory>

namespace docscan {

/// Black & white: every pixel becomes pure black or pure white.
class BwFilter final : public Filter {
public:
    static FilterInfo info();
    explicit BwFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    std::unique_ptr<Binarizer> binarizer_;
};

} // namespace docscan
