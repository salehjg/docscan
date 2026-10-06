#include "filters/ContrastFilter.hpp"

#include "vision/Imaging.hpp"

#include <opencv2/imgproc.hpp>

#include <array>

namespace docscan {

namespace {

/// Darkest and brightest levels after ignoring `clip` of the pixels at each end.
std::pair<int, int> levels(const cv::Mat& gray, double clip)
{
    std::array<double, 256> histogram{};
    for (auto it = gray.begin<uchar>(); it != gray.end<uchar>(); ++it)
        ++histogram[*it];

    const double limit = clip * double(gray.total());
    int low = 0;
    for (double count = histogram[0]; low < 255 && count <= limit; count += histogram[++low]) {}
    int high = 255;
    for (double count = histogram[255]; high > 0 && count <= limit; count += histogram[--high]) {}
    return high > low ? std::pair{low, high} : std::pair{0, 255};
}

} // namespace

FilterInfo ContrastFilter::info()
{
    return {"contrast",
            "stretch levels: ink black, paper white",
            {ParamSpec::number("amount", 1, 0, 4, "extra contrast on top (1 = none, 1.5 = punchy)"),
             ParamSpec::number("clip", 1, 0, 20, "percent of darkest and brightest pixels allowed to clip")}};
}

ContrastFilter::ContrastFilter(const Settings& settings)
    : amount_(settings.get<double>("amount")), clip_(settings.get<double>("clip") / 100.0)
{
}

cv::Mat ContrastFilter::apply(const cv::Mat& image) const
{
    const auto [low, high] = levels(toGray(image), clip_);
    cv::Mat table(1, 256, CV_8U);
    for (int level = 0; level < 256; ++level) {
        const double stretched = (level - low) * 255.0 / (high - low);
        table.at<uchar>(level) = cv::saturate_cast<uchar>((stretched - 127.5) * amount_ + 127.5);
    }
    cv::Mat result;
    cv::LUT(image, table, result);
    return result;
}

} // namespace docscan
