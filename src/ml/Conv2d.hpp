#pragma once

#include "ml/TorchCheckpoint.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace docscan {

/// Activations of a convolutional network for one image: `channels` planes of `height` x `width` floats,
/// one plane after the other (PyTorch's NCHW layout for a batch of one).
struct FeatureMap {
    int channels = 0;
    int height = 0;
    int width = 0;
    std::vector<float> data;

    FeatureMap() = default;
    FeatureMap(int channels, int height, int width);

    float* plane(int channel) { return data.data() + std::size_t(channel) * height * width; }
    const float* plane(int channel) const { return data.data() + std::size_t(channel) * height * width; }
};

/// `parts` stacked along the channels, like torch.cat(parts, dim=1).
FeatureMap concatenate(const std::vector<FeatureMap>& parts);

/// A 2D convolution with an optional BatchNorm (folded into it) and activation after it, for inference on the CPU.
/// The work is split into tiles of output pixels across OpenMP threads; each tile gathers its input patches
/// (im2col) and multiplies them by the weights with a small register-blocked kernel.
class Conv2d {
public:
    enum class Padding { Zeros, Reflect };
    enum class Activation { None, Relu, PRelu };

    struct Shape {
        int kernel;
        int stride = 1;
        int dilation = 1;
        Padding padding = Padding::Zeros;
    };

    /// Reads `conv`.weight (and `conv`.bias if present) from `weights`, and the BatchNorm `batchNorm`.weight,
    /// .bias, .running_mean and .running_var unless `batchNorm` is empty. A PReLU's single slope is read from
    /// `prelu`.weight. Padding keeps the size at stride 1, as `padding = dilation * (kernel // 2)` does.
    Conv2d(const std::map<std::string, Tensor>& weights, const std::string& conv, const std::string& batchNorm,
           Shape shape, Activation activation = Activation::None, const std::string& prelu = {});

    int outputs() const { return outputs_; }

    /// The layer applied to `input`; `residual`, when given, is added before the activation.
    FeatureMap operator()(const FeatureMap& input, const FeatureMap* residual = nullptr) const;

private:
    int inputs_;
    int outputs_;
    Shape shape_;
    int pad_;
    Activation activation_;
    float slope_ = 0;
    std::vector<float> weights_; ///< BatchNorm folded in; packed for the kernel in blocks of output channels
    std::vector<float> bias_;    ///< one per output channel, padded to whole blocks
};

} // namespace docscan
