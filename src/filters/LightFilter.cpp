#include "filters/LightFilter.hpp"

#include "vision/Imaging.hpp"

#include <opencv2/imgproc.hpp>

namespace docscan {

namespace {

constexpr int kWorkSize = 512; // the background is smooth, so a small copy is enough to estimate it

/// The paper's brightness at every pixel: the page with the ink removed and smoothed out.
cv::Mat estimateBackground(const cv::Mat& image)
{
    cv::Mat background = shrink(image, shrinkFactor(image.size(), kWorkSize));
    cv::dilate(background, background, cv::getStructuringElement(cv::MORPH_ELLIPSE, {7, 7}));
    cv::medianBlur(background, background, 21);
    cv::resize(background, background, image.size(), 0, 0, cv::INTER_LINEAR);
    return background;
}

} // namespace

FilterInfo LightFilter::info()
{
    return {"light",
            "even out lighting and shadows, make the paper white",
            {ParamSpec::number("strength", 1, 0, 1, "0 = off, 1 = full effect")}};
}

LightFilter::LightFilter(const Settings& settings) : strength_(settings.get<double>("strength")) {}

cv::Mat LightFilter::apply(const cv::Mat& image) const
{
    if (strength_ == 0)
        return image;
    cv::Mat flat;
    cv::divide(image, estimateBackground(image), flat, 255.0);
    if (strength_ < 1)
        cv::addWeighted(image, 1.0 - strength_, flat, strength_, 0, flat);
    return flat;
}

} // namespace docscan
