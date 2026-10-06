#include "filters/DenoiseFilter.hpp"

#include <opencv2/photo.hpp>

namespace docscan {

FilterInfo DenoiseFilter::info()
{
    return {"denoise",
            "remove grain from dark, noisy photos (slow on big pages)",
            {ParamSpec::number("strength", 5, 1, 30, "higher removes more noise but also fine detail")}};
}

DenoiseFilter::DenoiseFilter(const Settings& settings) : strength_(float(settings.get<double>("strength"))) {}

cv::Mat DenoiseFilter::apply(const cv::Mat& image) const
{
    cv::Mat denoised;
    if (image.channels() == 3)
        cv::fastNlMeansDenoisingColored(image, denoised, strength_, strength_);
    else
        cv::fastNlMeansDenoising(image, denoised, strength_);
    return denoised;
}

} // namespace docscan
