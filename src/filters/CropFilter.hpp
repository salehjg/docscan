#pragma once

#include "filters/Filter.hpp"
#include "vision/PageFinder.hpp"
#include "vision/UVDocPageFinder.hpp"

#include <memory>
#include <optional>

namespace docscan {

/// Finds the page in the photo, cuts away the background and undoes the perspective,
/// so the page comes out flat and rectangular. With the UVDoc network it also flattens curled and folded
/// pages; otherwise it looks for the page's four straight edges.
class CropFilter final : public Filter {
public:
    static FilterInfo info();
    explicit CropFilter(const Settings& settings);
    cv::Mat apply(const cv::Mat& image) const override;

private:
    /// Size in pixels of the straightened page, before trimming. `apparent` is its size as photographed;
    /// `outline` its corners, which are the paper's own unless the page is `cutOff` by the photo's border.
    cv::Size2d pageSize(const Quad& outline, cv::Size2d apparent, bool cutOff, cv::Size imageSize) const;
    cv::Mat cropOutline(const cv::Mat& image) const;
    cv::Mat cropMesh(const cv::Mat& image, const PageMesh& mesh) const;

    std::unique_ptr<PageFinder> finder_;
    std::unique_ptr<UVDocPageFinder> network_; ///< used first when set
    std::optional<double> aspect_; ///< fixed long/short side ratio; empty to measure it
    double trim_;                  ///< fraction cut off each edge
};

} // namespace docscan
