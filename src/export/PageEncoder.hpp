#pragma once

#include <opencv2/core.hpp>

#include <filesystem>

namespace docscan {

/// Picks a file format per page: 1-bit PNG for black & white pages, JPEG for everything else.
class PageEncoder {
public:
    explicit PageEncoder(int jpegQuality);

    /// Writes `image` to `stem` plus the chosen extension and returns the path written.
    std::filesystem::path write(const cv::Mat& image, const std::filesystem::path& stem) const;

    /// True when the image holds nothing but pure black and pure white.
    static bool isBilevel(const cv::Mat& image);

private:
    int quality_;
};

} // namespace docscan
