#pragma once

#include "export/Exporter.hpp"
#include "export/PageEncoder.hpp"

#include <filesystem>

namespace docscan {

/// Saves every finished page as an image file: page-001.jpg, page-002.png, ...
class ImageExporter final : public Exporter {
public:
    ImageExporter(std::filesystem::path directory, int quality);

    void begin(std::size_t pageCount) override;
    void addPage(std::size_t index, const cv::Mat& page) override;
    void finish() override {}

private:
    std::filesystem::path directory_;
    PageEncoder encoder_;
};

} // namespace docscan
