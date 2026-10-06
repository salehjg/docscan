#include "vision/Imaging.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace docscan {

cv::Mat toGray(const cv::Mat& image)
{
    if (image.channels() == 1)
        return image;
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

double shrinkFactor(cv::Size size, int longSide)
{
    return std::min(1.0, double(longSide) / std::max(size.width, size.height));
}

cv::Mat shrink(const cv::Mat& image, double factor)
{
    if (factor >= 1.0)
        return image;
    cv::Mat small;
    cv::resize(image, small, {}, factor, factor, cv::INTER_AREA);
    return small;
}

cv::Mat whiteness(const cv::Mat& image)
{
    if (image.channels() == 1)
        return image;
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);
    cv::Mat channels[3];
    cv::split(hsv, channels);
    cv::Mat white;
    cv::subtract(channels[2], channels[1], white);
    return white;
}

cv::Mat eraseDetail(const cv::Mat& image)
{
    cv::Mat plain;
    cv::morphologyEx(image, plain, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_RECT, {9, 9}), {-1, -1}, 2);
    cv::GaussianBlur(plain, plain, {5, 5}, 0);
    return plain;
}

} // namespace docscan
