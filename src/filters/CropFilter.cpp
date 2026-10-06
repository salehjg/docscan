#include "filters/CropFilter.hpp"

#include "core/Error.hpp"
#include "core/Log.hpp"
#include "vision/ContourPageFinder.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <numbers>
#include <string_view>

namespace docscan {

namespace {

struct PaperShape {
    std::string_view name;
    double ratio; ///< long side / short side
};

constexpr std::array kPaperShapes{
    PaperShape{"a4", std::numbers::sqrt2}, // every ISO A and B size
    PaperShape{"letter", 11.0 / 8.5},
    PaperShape{"legal", 14.0 / 8.5},
};

constexpr double kSnapTolerance = 0.04; // a measured shape this close to a paper size is taken to be it
constexpr double kMaxCorrection = 1.5;  // distrust an estimate this far from the page's apparent shape

std::optional<double> parseAspect(const std::string& text)
{
    if (text == "auto")
        return std::nullopt;
    for (const auto& shape : kPaperShapes) {
        if (text == shape.name)
            return shape.ratio;
    }
    double ratio = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), ratio);
    if (error != std::errc{} || end != text.data() + text.size() || ratio <= 0)
        throw UsageError(
            std::format("crop.aspect: expected auto, a4, letter, legal or a number like 1.5, got '{}'", text));
    return std::max(ratio, 1.0 / ratio);
}

/// `longToShort` turned to match a page that measures `heightToWidth`; returns height / width.
double oriented(double longToShort, double heightToWidth)
{
    return heightToWidth >= 1 ? longToShort : 1.0 / longToShort;
}

/// The paper shape `heightToWidth` is close to, if any, oriented the same way.
std::optional<double> snapToPaper(double heightToWidth)
{
    const double longToShort = std::max(heightToWidth, 1.0 / heightToWidth);
    for (const auto& shape : kPaperShapes) {
        if (std::abs(longToShort / shape.ratio - 1.0) <= kSnapTolerance) {
            Log::detail(std::format("page shape {:.3f} looks like {}", longToShort, shape.name));
            return oriented(shape.ratio, heightToWidth);
        }
    }
    return std::nullopt;
}

} // namespace

FilterInfo CropFilter::info()
{
    return {"crop",
            "find the page, cut away the background and straighten it",
            {ParamSpec::text("aspect", "auto", "page shape: auto, a4, letter, legal or a ratio like 1.5"),
             ParamSpec::number("trim", 0.5, 0, 20, "percent cut off each edge, to remove leftover background")}};
}

CropFilter::CropFilter(const Settings& settings)
    : finder_(std::make_unique<ContourPageFinder>()),
      aspect_(parseAspect(settings.get<std::string>("aspect"))),
      trim_(settings.get<double>("trim") / 100.0)
{
}

cv::Mat CropFilter::apply(const cv::Mat& image) const
{
    const std::optional<Quad> page = finder_->find(image);
    if (!page) {
        Log::warn("no page found, keeping the whole photo");
        return image;
    }

    // Map the page onto a rectangle that sticks out by the trim margin, so the margin is cut off.
    const cv::Size2d full = pageSize(*page, image.size());
    const float marginX = float(trim_ * full.width);
    const float marginY = float(trim_ * full.height);
    const float right = float(full.width) - marginX;
    const float bottom = float(full.height) - marginY;
    const std::array<cv::Point2f, 4> target{
        cv::Point2f(-marginX, -marginY), {right, -marginY}, {right, bottom}, {-marginX, bottom}};
    const cv::Size size(cvRound(full.width - 2 * marginX), cvRound(full.height - 2 * marginY));

    const cv::Mat transform = cv::getPerspectiveTransform(page->corners().data(), target.data());
    cv::Mat flat;
    cv::warpPerspective(image, flat, transform, size, cv::INTER_CUBIC, cv::BORDER_REPLICATE);
    Log::detail(std::format("page found, straightened to {}x{}", flat.cols, flat.rows));
    return flat;
}

cv::Size2d CropFilter::pageSize(const Quad& page, cv::Size imageSize) const
{
    double width = page.width();
    double height = page.height();
    const double apparent = height / width;

    double aspect = apparent;
    if (aspect_) {
        aspect = oriented(*aspect_, apparent);
    } else {
        // Only a page that is fully in view can be recognized as a paper size.
        const double estimate = page.trueAspect(imageSize);
        if (std::abs(std::log(estimate / apparent)) <= std::log(kMaxCorrection))
            aspect = page.touchesBorder(imageSize) ? estimate : snapToPaper(estimate).value_or(estimate);
    }

    // Keep the resolution of the sharper direction and stretch the other one to the right shape.
    if (height >= width * aspect)
        width = height / aspect;
    else
        height = width * aspect;
    return {width, height};
}

} // namespace docscan
