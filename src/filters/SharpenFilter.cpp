#include "filters/SharpenFilter.hpp"

#include <opencv2/imgproc.hpp>

namespace docscan {

FilterInfo SharpenFilter::info()
{
    return {"sharpen",
            "make text edges crisper",
            {ParamSpec::number("amount", 0.6, 0, 5, "how strong (0 = off)"),
             ParamSpec::number("radius", 1.0, 0.1, 20, "size of the details to sharpen, in pixels")}};
}

SharpenFilter::SharpenFilter(const Settings& settings)
    : amount_(settings.get<double>("amount")), radius_(settings.get<double>("radius"))
{
}

cv::Mat SharpenFilter::apply(const cv::Mat& image) const
{
    if (amount_ == 0)
        return image;
    cv::Mat blurred;
    cv::GaussianBlur(image, blurred, {}, radius_);
    cv::Mat sharpened;
    cv::addWeighted(image, 1.0 + amount_, blurred, -amount_, 0, sharpened);
    return sharpened;
}

} // namespace docscan
