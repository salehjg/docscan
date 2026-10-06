#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Unsharp mask: makes text edges crisper.
class SharpenFilter final : public Filter {
public:
    static FilterInfo info();
    explicit SharpenFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    double amount_;
    double radius_;
};

} // namespace docscan
