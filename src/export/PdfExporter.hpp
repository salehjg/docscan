#pragma once

#include "export/Exporter.hpp"
#include "export/PageEncoder.hpp"
#include "export/PaperSize.hpp"
#include "export/TempDir.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace docscan {

/// Builds the PDF with pdfTeX: each page is encoded into a temporary directory, a small plain-TeX
/// document places the images on their pages, and pdftex copies them into the PDF as they are.
class PdfExporter final : public Exporter {
public:
    PdfExporter(std::filesystem::path output, PaperSize paper, int quality);

    void begin(std::size_t pageCount) override;
    void addPage(std::size_t index, const cv::Mat& page) override;
    void finish() override;

private:
    struct PageFile {
        std::filesystem::path name;
        cv::Size size;
    };

    std::string texDocument() const;

    std::filesystem::path output_;
    PaperSize paper_;
    PageEncoder encoder_;
    std::filesystem::path pdftex_;
    std::optional<TempDir> workDir_;
    std::vector<PageFile> pages_;
};

} // namespace docscan
