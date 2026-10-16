#include "ml/Conv2d.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <stdexcept>

namespace docscan {

namespace {

constexpr int kRows = 4;     // output channels computed together
constexpr int kLanes = 8;    // floats in a vector register (AVX); narrower hardware splits it
constexpr int kColumns = 16; // output pixels computed together: two vectors
constexpr int kMaxTile = 128;
constexpr float kBatchNormEpsilon = 1e-5f; // PyTorch's default

using Vec = float __attribute__((vector_size(kLanes * sizeof(float))));

const Tensor& find(const std::map<std::string, Tensor>& weights, const std::string& layer, const char* parameter)
{
    const std::string name = layer + "." + parameter;
    const auto found = weights.find(name);
    if (found == weights.end())
        throw std::runtime_error(std::format("model checkpoint: missing {}", name));
    return found->second;
}

/// `index` mirrored back into [0, size) without repeating the edge, like PyTorch's reflect padding.
int reflect(int index, int size)
{
    if (size == 1)
        return 0;
    while (index < 0 || index >= size)
        index = index < 0 ? -index : 2 * (size - 1) - index;
    return index;
}

// The multiplication is also built for x86-64-v3 (AVX2 and FMA), picked when the program starts:
// eight times faster than the baseline on every x86-64 CPU made in the last decade.
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__)
#define DOCSCAN_VECTOR_CLONES __attribute__((target_clones("arch=x86-64-v3", "default")))
#else
#define DOCSCAN_VECTOR_CLONES
#endif

/// sums[o][i] = sum over d of weights[o][d] * patches[d][i], for the first `count` (a multiple of kColumns)
/// of the `tile` pixels in each row of `patches`. `weights` are packed in blocks of kRows output channels.
DOCSCAN_VECTOR_CLONES
void multiply(const float* weights, const float* patches, int blocks, int depth, int tile, int count, float* sums)
{
    for (int b = 0; b < blocks; ++b) {
        const float* w = weights + std::size_t(b) * depth * kRows;
        for (int j = 0; j < count; j += kColumns) {
            Vec acc[kRows][2] = {};
            const float* p = patches + j;
            for (int d = 0; d < depth; ++d, p += tile) {
                Vec p0;
                Vec p1;
                std::memcpy(&p0, p, sizeof p0);
                std::memcpy(&p1, p + kLanes, sizeof p1);
                for (int r = 0; r < kRows; ++r) {
                    const Vec wr = Vec{} + w[d * kRows + r];
                    acc[r][0] += wr * p0;
                    acc[r][1] += wr * p1;
                }
            }
            for (int r = 0; r < kRows; ++r)
                std::memcpy(sums + std::size_t(b * kRows + r) * tile + j, acc[r], sizeof acc[r]);
        }
    }
}

} // namespace

FeatureMap::FeatureMap(int channels, int height, int width)
    : channels(channels), height(height), width(width), data(std::size_t(channels) * height * width)
{
}

FeatureMap concatenate(const std::vector<FeatureMap>& parts)
{
    int channels = 0;
    for (const auto& part : parts) {
        if (part.height != parts.front().height || part.width != parts.front().width)
            throw std::logic_error("concatenate: feature maps of different sizes");
        channels += part.channels;
    }
    FeatureMap out(channels, parts.front().height, parts.front().width);
    auto* dst = out.data.data();
    for (const auto& part : parts)
        dst = std::copy(part.data.begin(), part.data.end(), dst);
    return out;
}

Conv2d::Conv2d(const std::map<std::string, Tensor>& weights, const std::string& conv, const std::string& batchNorm,
               Shape shape, Activation activation, const std::string& prelu)
    : shape_(shape), pad_(shape.dilation * (shape.kernel / 2)), activation_(activation)
{
    const Tensor& weight = find(weights, conv, "weight");
    if (weight.shape.size() != 4 || weight.shape[2] != shape.kernel || weight.shape[3] != shape.kernel)
        throw std::runtime_error(std::format("model checkpoint: {}.weight has an unexpected shape", conv));
    outputs_ = int(weight.shape[0]);
    inputs_ = int(weight.shape[1]);

    std::vector<float> scale(std::size_t(outputs_), 1.0f);
    std::vector<float> shift(std::size_t(outputs_), 0.0f);
    if (const auto bias = weights.find(conv + ".bias"); bias != weights.end())
        shift = bias->second.data;
    if (!batchNorm.empty()) {
        // y = (conv(x) + b - mean) * gamma / sqrt(var + eps) + beta, folded into the convolution.
        const auto& gamma = find(weights, batchNorm, "weight").data;
        const auto& beta = find(weights, batchNorm, "bias").data;
        const auto& mean = find(weights, batchNorm, "running_mean").data;
        const auto& var = find(weights, batchNorm, "running_var").data;
        for (int o = 0; o < outputs_; ++o) {
            scale[o] = gamma[o] / std::sqrt(var[o] + kBatchNormEpsilon);
            shift[o] = (shift[o] - mean[o]) * scale[o] + beta[o];
        }
    }
    if (activation == Activation::PRelu)
        slope_ = find(weights, prelu, "weight").data.at(0);

    const int depth = inputs_ * shape.kernel * shape.kernel;
    const int blocks = (outputs_ + kRows - 1) / kRows;
    weights_.assign(std::size_t(blocks) * depth * kRows, 0.0f);
    bias_.assign(std::size_t(blocks) * kRows, 0.0f);
    for (int o = 0; o < outputs_; ++o) {
        for (int d = 0; d < depth; ++d)
            weights_[(std::size_t(o / kRows) * depth + d) * kRows + o % kRows] =
                weight.data[std::size_t(o) * depth + d] * scale[o];
        bias_[o] = shift[o];
    }
}

FeatureMap Conv2d::operator()(const FeatureMap& input, const FeatureMap* residual) const
{
    if (input.channels != inputs_)
        throw std::logic_error("Conv2d: wrong number of input channels");
    const int k = shape_.kernel;
    const int stride = shape_.stride;
    const int dilation = shape_.dilation;
    const int height = (input.height + 2 * pad_ - dilation * (k - 1) - 1) / stride + 1;
    const int width = (input.width + 2 * pad_ - dilation * (k - 1) - 1) / stride + 1;
    FeatureMap output(outputs_, height, width);
    if (residual && (residual->channels != outputs_ || residual->height != height || residual->width != width))
        throw std::logic_error("Conv2d: residual of the wrong size");

    const int pixels = height * width;
    const int depth = inputs_ * k * k;
    const int blocks = (outputs_ + kRows - 1) / kRows;
    // Smaller tiles on smaller maps, so that every thread gets work.
    const int tile = std::clamp((pixels / 64 + kColumns - 1) / kColumns * kColumns, kColumns, kMaxTile);
    const int tiles = (pixels + tile - 1) / tile;
    const bool reflectPadding = shape_.padding == Padding::Reflect;

#pragma omp parallel
    {
        std::vector<float> patches(std::size_t(depth) * std::size_t(tile));
        std::vector<float> sums(std::size_t(blocks) * kRows * std::size_t(tile));
#pragma omp for schedule(dynamic)
        for (int t = 0; t < tiles; ++t) {
            const int first = t * tile;
            const int count = std::min(tile, pixels - first);

            // im2col: row (c, ky, kx) of `patches` holds that input pixel for each output pixel of the tile,
            // copied a run of pixels along an output row at a time; only the ends of a run reach into the padding.
            for (int c = 0; c < inputs_; ++c) {
                const float* plane = input.plane(c);
                for (int ky = 0; ky < k; ++ky) {
                    for (int kx = 0; kx < k; ++kx) {
                        float* row = &patches[(std::size_t(c * k + ky) * k + kx) * tile];
                        for (int i = 0; i < count;) {
                            const int oy = (first + i) / width;
                            const int ox = (first + i) % width;
                            const int run = std::min(count - i, width - ox);
                            const int y = oy * stride - pad_ + ky * dilation;
                            if ((y < 0 || y >= input.height) && !reflectPadding) {
                                std::fill(row + i, row + i + run, 0.0f);
                                i += run;
                                continue;
                            }
                            const float* line = plane + std::size_t(reflect(y, input.height)) * input.width;
                            const int x0 = ox * stride - pad_ + kx * dilation;
                            // Pixels j of the run with 0 <= x0 + j * stride < input.width.
                            const int begin = std::clamp((stride - 1 - x0) / stride, 0, run);
                            const int last = input.width - 1 - x0; // integer division rounds to zero, not down
                            const int end = last < 0 ? begin : std::clamp(last / stride + 1, begin, run);
                            const auto padding = [&](int j) {
                                return reflectPadding ? line[reflect(x0 + j * stride, input.width)] : 0.0f;
                            };
                            for (int j = 0; j < begin; ++j)
                                row[i + j] = padding(j);
                            if (stride == 1)
                                std::copy(line + x0 + begin, line + x0 + end, row + i + begin);
                            else
                                for (int j = begin; j < end; ++j)
                                    row[i + j] = line[x0 + j * stride];
                            for (int j = end; j < run; ++j)
                                row[i + j] = padding(j);
                            i += run;
                        }
                        std::fill(row + count, row + tile, 0.0f);
                    }
                }
            }

            const int columns = (count + kColumns - 1) / kColumns * kColumns;
            multiply(weights_.data(), patches.data(), blocks, depth, tile, columns, sums.data());

            for (int o = 0; o < outputs_; ++o) {
                const float* sum = &sums[std::size_t(o) * tile];
                const float* add = residual ? residual->plane(o) + first : nullptr;
                float* out = output.plane(o) + first;
                for (int i = 0; i < count; ++i) {
                    float v = sum[i] + bias_[o];
                    if (add)
                        v += add[i];
                    if (activation_ == Activation::Relu)
                        v = std::max(v, 0.0f);
                    else if (activation_ == Activation::PRelu && v < 0)
                        v *= slope_;
                    out[i] = v;
                }
            }
        }
    }
    return output;
}

} // namespace docscan
