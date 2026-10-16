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

/// The UVDoc weights, downloaded to the user's data folder; nothing, with a warning, when that fails.
std::optional<std::filesystem::path> downloadModel(const std::string& method)
{
    try {
        return UVDocPageFinder::download();
    } catch (const Error& e) {
        if (method == "uvdoc")
            Log::warn(e.what());
        else
            Log::warn(std::format("{}; crop looks for the page's edges instead", e.what()));
        return std::nullopt;
    }
}

std::unique_ptr<UVDocPageFinder> makeNetwork(const std::string& method, const std::string& model)
{
    if (method == "contour")
        return nullptr;
    auto path = UVDocPageFinder::locate(model);
    if (!path && UVDocPageFinder::downloadAllowed())
        path = downloadModel(method);
    if (!path) {
        if (method == "uvdoc")
            throw UsageError("crop.method=uvdoc: the UVDoc model is not installed; download best_model.pkl from "
                             "github.com/tanguymagne/UVDoc and save it as ~/.local/share/docscan/uvdoc.pkl, "
                             "or point to it with -s crop.model=FILE");
        Log::detail("UVDoc model not installed: crop looks for the page's edges");
        return nullptr;
    }
    try {
        return std::make_unique<UVDocPageFinder>(*path);
    } catch (const std::exception& e) {
        throw UsageError(std::format("crop.model: {}", e.what()));
    }
}

} // namespace

FilterInfo CropFilter::info()
{
    return {"crop",
            "find the page, cut away the background and straighten it",
            {ParamSpec::text("aspect", "auto", "page shape: auto, a4, letter, legal or a ratio like 1.5"),
             ParamSpec::number("trim", 0.5, 0, 20, "percent cut off each edge, to remove leftover background"),
             ParamSpec::choice("method", {"auto", "uvdoc", "contour"},
                               "uvdoc (neural network, also flattens curled and folded pages), contour (the page's "
                               "straight edges), auto (uvdoc when its model is installed or can be downloaded)"),
             ParamSpec::text("model", "",
                             "UVDoc weights file; empty: ~/.local/share/docscan (downloaded on first use) or the "
                             "installed one")}};
}

CropFilter::CropFilter(const Settings& settings)
    : finder_(std::make_unique<ContourPageFinder>()),
      network_(makeNetwork(settings.get<std::string>("method"), settings.get<std::string>("model"))),
      aspect_(parseAspect(settings.get<std::string>("aspect"))),
      trim_(settings.get<double>("trim") / 100.0)
{
}

cv::Mat CropFilter::apply(const cv::Mat& image) const
{
    if (network_) {
        if (const auto mesh = network_->find(image))
            return cropMesh(image, *mesh);
        Log::detail("the network found no page, looking for its edges");
    }
    return cropOutline(image);
}

cv::Mat CropFilter::cropMesh(const cv::Mat& image, const PageMesh& mesh) const
{
    const cv::Size2d full = pageSize(mesh.outline(), mesh.size(), mesh.touchesBorder(image.size()), image.size());
    const cv::Size size(std::max(1, cvRound(full.width * (1 - 2 * trim_))),
                        std::max(1, cvRound(full.height * (1 - 2 * trim_))));
    cv::Mat flat = mesh.unwarp(image, size, trim_);
    Log::detail(std::format("page found by the network, flattened to {}x{}", flat.cols, flat.rows));
    return flat;
}

cv::Mat CropFilter::cropOutline(const cv::Mat& image) const
{
    const std::optional<Quad> page = finder_->find(image);
    if (!page) {
        Log::warn("no page found, keeping the whole photo");
        return image;
    }

    // Map the page onto a rectangle that sticks out by the trim margin, so the margin is cut off.
    const cv::Size2d full =
        pageSize(*page, {page->width(), page->height()}, page->touchesBorder(image.size()), image.size());
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

cv::Size2d CropFilter::pageSize(const Quad& outline, cv::Size2d apparentSize, bool cutOff, cv::Size imageSize) const
{
    double width = apparentSize.width;
    double height = apparentSize.height;
    const double apparent = height / width;

    double aspect = apparent;
    if (aspect_) {
        aspect = oriented(*aspect_, apparent);
    } else if (!cutOff) {
        // Only a page that is fully in view has four real corners to measure its shape from:
        // where the photo cuts it off, the outline is the frame's, not a rectangle on the paper.
        const double estimate = outline.trueAspect(imageSize);
        if (std::abs(std::log(estimate / apparent)) <= std::log(kMaxCorrection))
            aspect = snapToPaper(estimate).value_or(estimate);
    }

    // Keep the resolution of the sharper direction and stretch the other one to the right shape.
    if (height >= width * aspect)
        width = height / aspect;
    else
        height = width * aspect;
    return {width, height};
}

} // namespace docscan
