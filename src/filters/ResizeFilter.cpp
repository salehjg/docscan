#include "filters/ResizeFilter.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace docscan {

FilterInfo ResizeFilter::info()
{
    return {"resize",
            "change the size",
            {ParamSpec::integer("max", 2000, 0, 100000, "longest side in pixels; only ever shrinks (0 = off)"),
             ParamSpec::integer("width", 0, 0, 100000, "width in pixels; keeps the shape unless height is set too"),
             ParamSpec::integer("height", 0, 0, 100000, "height in pixels; keeps the shape unless width is set too"),
             ParamSpec::number("scale", 0, 0, 10, "scale factor such as 0.5 (0 = off)")}};
}

ResizeFilter::ResizeFilter(const Settings& settings)
    : max_(settings.get<int>("max")),
      width_(settings.get<int>("width")),
      height_(settings.get<int>("height")),
      scale_(settings.get<double>("scale"))
{
}

cv::Mat ResizeFilter::apply(const cv::Mat& image) const
{
    const cv::Size size = targetSize(image.size());
    if (size == image.size())
        return image;
    const bool shrinking = size.area() < image.size().area();
    cv::Mat resized;
    cv::resize(image, resized, size, 0, 0, shrinking ? cv::INTER_AREA : cv::INTER_CUBIC);
    return resized;
}

cv::Size ResizeFilter::targetSize(cv::Size size) const
{
    const auto scaled = [&](double factor) {
        return cv::Size(std::max(1, cvRound(size.width * factor)), std::max(1, cvRound(size.height * factor)));
    };

    if (scale_ > 0)
        return scaled(scale_);
    if (width_ > 0 && height_ > 0)
        return {width_, height_};
    if (width_ > 0)
        return scaled(double(width_) / size.width);
    if (height_ > 0)
        return scaled(double(height_) / size.height);
    if (const int longest = std::max(size.width, size.height); max_ > 0 && longest > max_)
        return scaled(double(max_) / longest);
    return size;
}

} // namespace docscan
