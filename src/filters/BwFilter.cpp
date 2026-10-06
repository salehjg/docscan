#include "filters/BwFilter.hpp"

#include "vision/Imaging.hpp"

namespace docscan {

namespace {

std::unique_ptr<Binarizer> makeBinarizer(const std::string& method, int window, double clean)
{
    if (method == "otsu")
        return std::make_unique<OtsuBinarizer>();
    if (method == "adaptive")
        return std::make_unique<AdaptiveBinarizer>(window, clean * 50.0);
    return std::make_unique<SauvolaBinarizer>(window, clean);
}

} // namespace

FilterInfo BwFilter::info()
{
    return {"bw",
            "black & white: each pixel becomes black or white",
            {ParamSpec::choice("method", {"sauvola", "otsu", "adaptive"},
                               "sauvola (local, best for text), otsu (one level for the page), adaptive (local mean)"),
             ParamSpec::integer("window", 41, 3, 999, "neighborhood size in pixels for sauvola and adaptive"),
             ParamSpec::number("clean", 0.2, 0, 1, "higher = cleaner paper but thinner text")}};
}

BwFilter::BwFilter(const Settings& settings)
    : binarizer_(makeBinarizer(settings.get<std::string>("method"), settings.get<int>("window"),
                               settings.get<double>("clean")))
{
}

cv::Mat BwFilter::apply(const cv::Mat& image) const { return binarizer_->binarize(toGray(image)); }

} // namespace docscan
