#pragma once

#include "ml/Conv2d.hpp"

#include <opencv2/core.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace docscan {

/// The document unwarping network of UVDoc (Verhoeven, Magne & Sorkine-Hornung, "UVDoc: Neural Grid-based
/// Document Unwarping", SIGGRAPH Asia 2023; github.com/tanguymagne/UVDoc, MIT license), run on the CPU.
/// From a photo it predicts where a regular grid laid over the flattened page lies in the photo; sampling
/// the photo along that grid both cuts away the background and undoes perspective, curls and folds.
class UVDocNet {
public:
    static constexpr int kInputWidth = 488;
    static constexpr int kInputHeight = 712;

    /// Loads the trained weights from the checkpoint the authors publish (model/best_model.pkl).
    explicit UVDocNet(const std::filesystem::path& checkpoint);

    /// The network's raw output for an RGB image of kInputWidth x kInputHeight in [0, 1]:
    /// two channels (x, y) of the grid points, in [-1, 1] across the image.
    FeatureMap forward(const FeatureMap& image) const;

    /// For a BGR photo of any size, the grid over the page: CV_32FC2, 45 rows of 31 points from the top of the
    /// page down, each point's position in the photo's pixel coordinates.
    cv::Mat predictGrid(const cv::Mat& photo) const;

private:
    struct ResidualBlock {
        Conv2d conv1;
        Conv2d conv2;
        std::optional<Conv2d> downsample;
    };

    std::vector<Conv2d> head_;
    std::vector<ResidualBlock> blocks_;
    std::vector<std::vector<Conv2d>> bridges_;
    std::vector<Conv2d> bridgeMerge_;
    std::vector<Conv2d> grid_;
};

} // namespace docscan
