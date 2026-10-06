#pragma once

#include <opencv2/core.hpp>

#include <cstddef>

namespace docscan {

/// Receives the finished pages and turns them into the output (a PDF, a folder of images, ...).
class Exporter {
public:
    Exporter() = default;
    Exporter(const Exporter&) = delete;
    Exporter& operator=(const Exporter&) = delete;
    virtual ~Exporter() = default;

    /// Called once before any page arrives; a good moment to fail early.
    virtual void begin(std::size_t pageCount) = 0;
    /// Receives page `index` (0-based). Called concurrently for different pages, in any order.
    virtual void addPage(std::size_t index, const cv::Mat& page) = 0;
    /// Called once after every page was added.
    virtual void finish() = 0;
};

} // namespace docscan
