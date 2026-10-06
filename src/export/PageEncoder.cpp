#include "export/PageEncoder.hpp"

#include "core/Error.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <format>
#include <vector>

namespace docscan {

PageEncoder::PageEncoder(int jpegQuality) : quality_(jpegQuality) {}

std::filesystem::path PageEncoder::write(const cv::Mat& image, const std::filesystem::path& stem) const
{
    const bool bilevel = isBilevel(image);
    std::filesystem::path path = stem;
    path += bilevel ? ".png" : ".jpg";
    const std::vector<int> options = bilevel
        ? std::vector<int>{cv::IMWRITE_PNG_BILEVEL, 1, cv::IMWRITE_PNG_COMPRESSION, 9}
        : std::vector<int>{cv::IMWRITE_JPEG_QUALITY, quality_, cv::IMWRITE_JPEG_OPTIMIZE, 1};
    if (!cv::imwrite(path.string(), image, options))
        throw ProcessingError(std::format("cannot write '{}'", path.string()));
    return path;
}

bool PageEncoder::isBilevel(const cv::Mat& image)
{
    if (image.channels() != 1 || image.depth() != CV_8U)
        return false;
    cv::Mat gray;
    cv::inRange(image, 1, 254, gray);
    return cv::countNonZero(gray) == 0;
}

} // namespace docscan
