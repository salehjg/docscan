#pragma once

#include "vision/EdgeSnapper.hpp"
#include "vision/PageFinder.hpp"

namespace docscan {

/// Finds the page as the best four-sided outline among the shapes of two maps of a shrunk copy:
/// the page border from edge detection, and the region of bright, colorless "paper" pixels.
/// Candidates are scored by size, by how rectangular they are and by how well their sides follow real edges.
/// The winner's sides are then snapped onto the paper's real edges for a tight crop.
class ContourPageFinder final : public PageFinder {
public:
    std::optional<Quad> find(const cv::Mat& image) const override;

private:
    EdgeSnapper snapper_;
};

} // namespace docscan
