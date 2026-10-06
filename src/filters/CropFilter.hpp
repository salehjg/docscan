#pragma once

#include "filters/Filter.hpp"
#include "vision/PageFinder.hpp"

#include <memory>
#include <optional>

namespace docscan {

/// Finds the page in the photo, cuts away the background and undoes the perspective,
/// so the page comes out flat and rectangular.
class CropFilter final : public Filter {
public:
    static FilterInfo info();
    explicit CropFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    /// Size in pixels of the straightened page, before trimming.
    cv::Size2d pageSize(const Quad& page, cv::Size imageSize) const;

    std::unique_ptr<PageFinder> finder_;
    std::optional<double> aspect_; ///< fixed long/short side ratio; empty to measure it
    double trim_;                  ///< fraction cut off each edge
};

} // namespace docscan
