#pragma once

#include "vision/Quad.hpp"

#include <opencv2/core.hpp>

#include <optional>

namespace docscan {

/// Locates the document page in a photo.
class PageFinder {
public:
    virtual ~PageFinder() = default;

    /// Corners of the page in `image`, or nothing when no page stands out.
    [[nodiscard]] virtual std::optional<Quad> find(const cv::Mat& image) const = 0;
};

} // namespace docscan
