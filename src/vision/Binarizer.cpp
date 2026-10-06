#include "vision/Binarizer.hpp"

#include <opencv2/imgproc.hpp>

namespace docscan {

namespace {

int oddAtLeast3(int window) { return std::max(3, window | 1); }

} // namespace

cv::Mat OtsuBinarizer::binarize(const cv::Mat& gray) const
{
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    return binary;
}

AdaptiveBinarizer::AdaptiveBinarizer(int window, double offset) : window_(oddAtLeast3(window)), offset_(offset) {}

cv::Mat AdaptiveBinarizer::binarize(const cv::Mat& gray) const
{
    cv::Mat binary;
    cv::adaptiveThreshold(gray, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, window_, offset_);
    return binary;
}

SauvolaBinarizer::SauvolaBinarizer(int window, double k) : window_(oddAtLeast3(window)), k_(k) {}

cv::Mat SauvolaBinarizer::binarize(const cv::Mat& gray) const
{
    constexpr double kDynamicRange = 128.0; // largest standard deviation an 8-bit image can have

    cv::Mat pixels;
    gray.convertTo(pixels, CV_32F);
    const cv::Size window(window_, window_);
    cv::Mat mean;
    cv::Mat meanOfSquares;
    cv::boxFilter(pixels, mean, CV_32F, window, {-1, -1}, true, cv::BORDER_REPLICATE);
    cv::sqrBoxFilter(pixels, meanOfSquares, CV_32F, window, {-1, -1}, true, cv::BORDER_REPLICATE);

    cv::Mat deviation = cv::max(meanOfSquares - mean.mul(mean), 0.0);
    cv::sqrt(deviation, deviation);

    const cv::Mat threshold = mean.mul(1.0 + k_ * (deviation / kDynamicRange - 1.0));
    cv::Mat binary;
    cv::compare(pixels, threshold, binary, cv::CMP_GT);
    return binary;
}

} // namespace docscan
