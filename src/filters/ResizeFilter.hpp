#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Scales the page. `scale` wins over `width`/`height`, which win over `max`.
class ResizeFilter final : public Filter {
public:
    static FilterInfo info();
    explicit ResizeFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    cv::Size targetSize(cv::Size size) const;

    int max_;
    int width_;
    int height_;
    double scale_;
};

} // namespace docscan
