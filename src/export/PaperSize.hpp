#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace docscan {

/// Where an image goes on a PDF page. Everything is in PostScript points, measured from the top-left corner.
struct Placement {
    double pageWidth;
    double pageHeight;
    double x;
    double y;
    double width;
    double height;
};

/// A paper format such as A4, or "fit", where every page takes the shape of its image.
class PaperSize {
public:
    /// Throws UsageError for unknown names.
    static PaperSize fromName(std::string_view name);
    static std::vector<std::string> names();

    /// Fits an image of `imageSize` pixels onto the paper, turned to the image's orientation and centered.
    Placement place(cv::Size imageSize) const;

private:
    PaperSize(double width, double height);

    double width_;  ///< portrait width in points; 0 for "fit"
    double height_; ///< portrait height in points; 0 for "fit"
};

} // namespace docscan
