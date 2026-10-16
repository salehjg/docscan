#include "ml/Conv2d.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <random>

using namespace docscan;

namespace {

using Activation = Conv2d::Activation;
using Padding = Conv2d::Padding;

std::mt19937 rng(42);

std::vector<float> randomValues(std::size_t n, float low, float high)
{
    std::uniform_real_distribution<float> dist(low, high);
    std::vector<float> v(n);
    for (auto& x : v)
        x = dist(rng);
    return v;
}

FeatureMap randomMap(int channels, int height, int width)
{
    FeatureMap map(channels, height, width);
    map.data = randomValues(map.data.size(), -1, 1);
    return map;
}

struct Layer {
    int inputs;
    int outputs;
    Conv2d::Shape shape;
    bool bias;
    bool batchNorm;
    Activation activation;
    std::map<std::string, Tensor> weights;
};

Layer makeLayer(int inputs, int outputs, Conv2d::Shape shape, bool bias, bool batchNorm, Activation activation)
{
    Layer layer{inputs, outputs, shape, bias, batchNorm, activation, {}};
    const auto n = std::size_t(outputs);
    const int k = shape.kernel;
    layer.weights["conv.weight"] = {{outputs, inputs, k, k}, randomValues(n * inputs * k * k, -0.5f, 0.5f)};
    if (bias)
        layer.weights["conv.bias"] = {{outputs}, randomValues(n, -0.5f, 0.5f)};
    if (batchNorm) {
        layer.weights["bn.weight"] = {{outputs}, randomValues(n, 0.5f, 1.5f)};
        layer.weights["bn.bias"] = {{outputs}, randomValues(n, -0.5f, 0.5f)};
        layer.weights["bn.running_mean"] = {{outputs}, randomValues(n, -0.5f, 0.5f)};
        layer.weights["bn.running_var"] = {{outputs}, randomValues(n, 0.1f, 2.0f)};
    }
    layer.weights["act.weight"] = {{1}, {0.25f}};
    return layer;
}

/// The textbook definition, one multiply-add at a time, as PyTorch computes it.
FeatureMap reference(const Layer& layer, const FeatureMap& x, const FeatureMap* residual)
{
    const auto& [kernel, stride, dilation, padding] = layer.shape;
    const int pad = dilation * (kernel / 2);
    const int height = (x.height + 2 * pad - dilation * (kernel - 1) - 1) / stride + 1;
    const int width = (x.width + 2 * pad - dilation * (kernel - 1) - 1) / stride + 1;
    const auto mirror = [](int i, int size) { return i < 0 ? -i : i >= size ? 2 * (size - 1) - i : i; };
    const auto& w = layer.weights.at("conv.weight").data;
    FeatureMap y(layer.outputs, height, width);
    for (int o = 0; o < layer.outputs; ++o) {
        for (int oy = 0; oy < height; ++oy) {
            for (int ox = 0; ox < width; ++ox) {
                double sum = layer.bias ? layer.weights.at("conv.bias").data[o] : 0.0;
                for (int c = 0; c < layer.inputs; ++c) {
                    for (int ky = 0; ky < kernel; ++ky) {
                        for (int kx = 0; kx < kernel; ++kx) {
                            int iy = oy * stride - pad + ky * dilation;
                            int ix = ox * stride - pad + kx * dilation;
                            const bool inside = iy >= 0 && iy < x.height && ix >= 0 && ix < x.width;
                            if (!inside && padding == Padding::Zeros)
                                continue;
                            iy = mirror(iy, x.height);
                            ix = mirror(ix, x.width);
                            sum += double(w[((o * layer.inputs + c) * kernel + ky) * kernel + kx])
                                 * x.plane(c)[iy * x.width + ix];
                        }
                    }
                }
                if (layer.batchNorm) {
                    const auto at = [&](const char* name) { return double(layer.weights.at(name).data[o]); };
                    sum = (sum - at("bn.running_mean")) / std::sqrt(at("bn.running_var") + 1e-5) * at("bn.weight")
                        + at("bn.bias");
                }
                if (residual)
                    sum += residual->plane(o)[oy * width + ox];
                if (layer.activation == Activation::Relu)
                    sum = std::max(sum, 0.0);
                else if (layer.activation == Activation::PRelu && sum < 0)
                    sum *= 0.25;
                y.plane(o)[oy * width + ox] = float(sum);
            }
        }
    }
    return y;
}

void checkAgainstReference(const Layer& layer, const FeatureMap& x, bool withResidual = false)
{
    const Conv2d conv(layer.weights, "conv", layer.batchNorm ? "bn" : "", layer.shape, layer.activation, "act");
    FeatureMap expected = reference(layer, x, nullptr);
    FeatureMap residual;
    if (withResidual) {
        residual = randomMap(expected.channels, expected.height, expected.width);
        expected = reference(layer, x, &residual);
    }
    const FeatureMap actual = conv(x, withResidual ? &residual : nullptr);

    REQUIRE(actual.channels == expected.channels);
    REQUIRE(actual.height == expected.height);
    REQUIRE(actual.width == expected.width);
    float worst = 0;
    for (std::size_t i = 0; i < actual.data.size(); ++i)
        worst = std::max(worst, std::abs(actual.data[i] - expected.data[i]));
    INFO(std::format("kernel {} stride {} dilation {}", layer.shape.kernel, layer.shape.stride, layer.shape.dilation));
    CHECK(worst < 1e-4f);
}

} // namespace

TEST_CASE("convolutions match the textbook definition")
{
    SECTION("5x5, stride 2, with bias, BatchNorm and ReLU")
    {
        checkAgainstReference(makeLayer(3, 8, {5, 2}, false, true, Activation::Relu), randomMap(3, 37, 29));
    }
    SECTION("dilated, with a residual added before the ReLU")
    {
        checkAgainstReference(makeLayer(6, 6, {5, 1, 3}, true, true, Activation::Relu), randomMap(6, 23, 31), true);
    }
    SECTION("dilation wider than the map, so most taps fall in the padding")
    {
        checkAgainstReference(makeLayer(4, 5, {3, 1, 18}, false, true, Activation::Relu), randomMap(4, 15, 11));
    }
    SECTION("reflect padding and PReLU, with fewer outputs than a block")
    {
        checkAgainstReference(makeLayer(7, 3, {5, 1, 1, Padding::Reflect}, false, true, Activation::PRelu),
                              randomMap(7, 12, 9));
    }
    SECTION("1x1, no BatchNorm, more pixels than one tile")
    {
        checkAgainstReference(makeLayer(9, 2, {1}, true, false, Activation::None), randomMap(9, 70, 50));
    }
}

TEST_CASE("feature maps are stacked along their channels")
{
    const FeatureMap a = randomMap(2, 3, 4);
    const FeatureMap b = randomMap(1, 3, 4);
    const FeatureMap both = concatenate({a, b});
    CHECK(both.channels == 3);
    CHECK(std::equal(a.data.begin(), a.data.end(), both.data.begin()));
    CHECK(std::equal(b.data.begin(), b.data.end(), both.plane(2)));
}
