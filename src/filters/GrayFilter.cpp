#include "filters/GrayFilter.hpp"

#include "vision/Imaging.hpp"

namespace docscan {

FilterInfo GrayFilter::info() { return {"gray", "convert to grayscale", {}, {"grey"}}; }

GrayFilter::GrayFilter(const Settings&) {}

cv::Mat GrayFilter::apply(const cv::Mat& image) const { return toGray(image); }

} // namespace docscan
