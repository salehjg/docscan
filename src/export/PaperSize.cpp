#include "export/PaperSize.hpp"

#include "core/Error.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace docscan {

namespace {

struct Format {
    std::string_view name;
    double width;
    double height;
};

constexpr std::array kFormats{
    Format{"a4", 595.276, 841.890},
    Format{"a5", 419.528, 595.276},
    Format{"letter", 612, 792},
    Format{"legal", 612, 1008},
    Format{"fit", 0, 0},
};

constexpr double kFitLongSide = 841.890; // "fit" pages are as long as A4

} // namespace

PaperSize::PaperSize(double width, double height) : width_(width), height_(height) {}

PaperSize PaperSize::fromName(std::string_view name)
{
    for (const auto& format : kFormats) {
        if (format.name == name)
            return {format.width, format.height};
    }
    throw UsageError(std::format("unknown page size '{}'{}; choose one of: {}", name,
                                 text::suggestion(name, names()), text::join(names(), ", ")));
}

std::vector<std::string> PaperSize::names()
{
    std::vector<std::string> names;
    for (const auto& format : kFormats)
        names.emplace_back(format.name);
    return names;
}

Placement PaperSize::place(cv::Size imageSize) const
{
    const double imageWidth = imageSize.width;
    const double imageHeight = imageSize.height;
    if (width_ == 0) {
        const double scale = kFitLongSide / std::max(imageWidth, imageHeight);
        const double width = imageWidth * scale;
        const double height = imageHeight * scale;
        return {width, height, 0, 0, width, height};
    }

    const bool landscape = imageWidth > imageHeight;
    const double pageWidth = landscape ? height_ : width_;
    const double pageHeight = landscape ? width_ : height_;
    const double scale = std::min(pageWidth / imageWidth, pageHeight / imageHeight);
    const double width = imageWidth * scale;
    const double height = imageHeight * scale;
    return {pageWidth, pageHeight, (pageWidth - width) / 2, (pageHeight - height) / 2, width, height};
}

} // namespace docscan
