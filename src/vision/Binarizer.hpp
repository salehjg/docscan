#pragma once

#include <opencv2/core.hpp>

namespace docscan {

/// Decides for every pixel whether it is ink or paper.
class Binarizer {
public:
    virtual ~Binarizer() = default;

    /// Takes 8-bit grayscale and returns an image holding only 0 (ink) and 255 (paper).
    [[nodiscard]] virtual cv::Mat binarize(const cv::Mat& gray) const = 0;
};

/// One threshold for the whole page (Otsu's method). Best after the lighting was evened out.
class OtsuBinarizer final : public Binarizer {
public:
    cv::Mat binarize(const cv::Mat& gray) const override;
};

/// Threshold from the Gaussian-weighted mean of each pixel's neighborhood, minus an offset.
class AdaptiveBinarizer final : public Binarizer {
public:
    AdaptiveBinarizer(int window, double offset);
    cv::Mat binarize(const cv::Mat& gray) const override;

private:
    int window_;
    double offset_;
};

/// Sauvola's method: a local threshold from the mean and the spread of each neighborhood.
/// Keeps text clean on flat paper and copes with stains and shading.
class SauvolaBinarizer final : public Binarizer {
public:
    SauvolaBinarizer(int window, double k);
    cv::Mat binarize(const cv::Mat& gray) const override;

private:
    int window_;
    double k_;
};

} // namespace docscan
