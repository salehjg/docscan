#include "ml/UVDocNet.hpp"

#include <opencv2/imgproc.hpp>

#include <format>

namespace docscan {

namespace {

using Activation = Conv2d::Activation;
using Padding = Conv2d::Padding;

constexpr int kFilters = 32;
constexpr int kKernel = 5;
constexpr int kLayerBlocks[] = {3, 4, 6};
constexpr int kLayerStrides[] = {1, 2, 2};
constexpr int kBlockDilation = 3; // inside a layer, after its first block
constexpr int kBridgeDilations[][3] = {{1}, {2}, {5}, {8, 3, 2}, {12, 7, 4}, {18, 12, 6}};

} // namespace

UVDocNet::UVDocNet(const std::filesystem::path& checkpoint)
{
    const auto w = loadTorchStateDict(checkpoint, "model_state");

    head_.emplace_back(w, "resnet_head.0", "resnet_head.1", Conv2d::Shape{kKernel, 2}, Activation::Relu);
    head_.emplace_back(w, "resnet_head.3", "resnet_head.4", Conv2d::Shape{kKernel, 2}, Activation::Relu);

    int channels = kFilters;
    for (int layer = 0; layer < 3; ++layer) {
        const int outputs = kFilters << layer;
        for (int i = 0; i < kLayerBlocks[layer]; ++i) {
            const auto name = std::format("resnet_down.layer{}.{}", layer + 1, i);
            // The first block of a layer changes size or channels; the others use dilated convolutions,
            // which PyTorch wraps in a Sequential (hence the ".0").
            const bool first = i == 0;
            const Conv2d::Shape shape = first ? Conv2d::Shape{kKernel, kLayerStrides[layer]}
                                              : Conv2d::Shape{kKernel, 1, kBlockDilation};
            const Conv2d::Shape second = first ? Conv2d::Shape{kKernel} : Conv2d::Shape{kKernel, 1, kBlockDilation};
            const std::string suffix = first ? "" : ".0";
            ResidualBlock block{Conv2d(w, name + ".conv1" + suffix, name + ".bn1", shape, Activation::Relu),
                                Conv2d(w, name + ".conv2" + suffix, name + ".bn2", second, Activation::Relu),
                                std::nullopt};
            if (first && (kLayerStrides[layer] != 1 || channels != outputs))
                block.downsample.emplace(w, name + ".downsample.0", name + ".downsample.1",
                                         Conv2d::Shape{kKernel, kLayerStrides[layer]});
            blocks_.push_back(std::move(block));
            channels = outputs;
        }
    }

    for (int b = 0; b < 6; ++b) {
        std::vector<Conv2d> bridge;
        for (int i = 0; i < 3 && kBridgeDilations[b][i] != 0; ++i) {
            const auto name = std::format("bridge_{}.{}", b + 1, i);
            bridge.emplace_back(w, name + ".0", name + ".1", Conv2d::Shape{3, 1, kBridgeDilations[b][i]},
                                Activation::Relu);
        }
        bridges_.push_back(std::move(bridge));
    }
    bridgeMerge_.emplace_back(w, "bridge_concat.0", "bridge_concat.1", Conv2d::Shape{1}, Activation::Relu);

    grid_.emplace_back(w, "out_point_positions2D.0", "out_point_positions2D.1",
                       Conv2d::Shape{kKernel, 1, 1, Padding::Reflect}, Activation::PRelu, "out_point_positions2D.2");
    grid_.emplace_back(w, "out_point_positions2D.3", "", Conv2d::Shape{kKernel, 1, 1, Padding::Reflect});
}

FeatureMap UVDocNet::forward(const FeatureMap& image) const
{
    FeatureMap x = head_[1](head_[0](image));
    for (const auto& block : blocks_) {
        const FeatureMap residual = block.downsample ? (*block.downsample)(x) : x;
        x = block.conv2(block.conv1(x), &residual);
    }

    std::vector<FeatureMap> branches;
    for (const auto& bridge : bridges_) {
        FeatureMap y = bridge.front()(x);
        for (std::size_t i = 1; i < bridge.size(); ++i)
            y = bridge[i](y);
        branches.push_back(std::move(y));
    }
    const FeatureMap merged = bridgeMerge_.front()(concatenate(branches));
    return grid_[1](grid_[0](merged));
}

cv::Mat UVDocNet::predictGrid(const cv::Mat& photo) const
{
    // Shrunk first, as 8-bit: converting the whole photo would cost more than the network's first layers.
    cv::Mat rgb;
    cv::resize(photo, rgb, {kInputWidth, kInputHeight}, 0, 0, cv::INTER_LINEAR);
    cv::cvtColor(rgb, rgb, rgb.channels() == 1 ? cv::COLOR_GRAY2RGB : cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255);

    FeatureMap image(3, kInputHeight, kInputWidth);
    cv::Mat planes[3];
    for (int c = 0; c < 3; ++c)
        planes[c] = cv::Mat(kInputHeight, kInputWidth, CV_32F, image.plane(c));
    cv::split(rgb, planes);

    const FeatureMap out = forward(image);
    cv::Mat grid(out.height, out.width, CV_32FC2);
    const float sx = float(photo.cols - 1) / 2;
    const float sy = float(photo.rows - 1) / 2;
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            const int i = y * out.width + x;
            // From [-1, 1] (grid_sample with align_corners=True) to pixel coordinates.
            grid.at<cv::Vec2f>(y, x) = {(out.plane(0)[i] + 1) * sx, (out.plane(1)[i] + 1) * sy};
        }
    }
    return grid;
}

} // namespace docscan
