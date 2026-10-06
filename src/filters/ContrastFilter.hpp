#pragma once

#include "filters/Filter.hpp"

namespace docscan {

/// Stretches the levels so the darkest ink becomes black and the paper white, then adds optional extra contrast.
class ContrastFilter final : public Filter {
public:
    static FilterInfo info();
    explicit ContrastFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    double amount_;
    double clip_; ///< fraction of pixels allowed to clip at each end
};

} // namespace docscan
