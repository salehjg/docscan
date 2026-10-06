#include "export/ImageExporter.hpp"

#include <format>

namespace docscan {

ImageExporter::ImageExporter(std::filesystem::path directory, int quality)
    : directory_(std::move(directory)), encoder_(quality)
{
}

void ImageExporter::begin(std::size_t) { std::filesystem::create_directories(directory_); }

void ImageExporter::addPage(std::size_t index, const cv::Mat& page)
{
    encoder_.write(page, directory_ / std::format("page-{:03}", index + 1));
}

} // namespace docscan
