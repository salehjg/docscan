#pragma once

#include "vision/PageMesh.hpp"

#include <opencv2/core.hpp>

namespace docscan {

/// Moves the outline of a page mesh onto the paper's real edges where the photo shows them. A network places
/// the outline only roughly and often leaves a sliver of table along a side, or stops short of the photo's
/// edge where the page runs off it. The edges are searched on the page flattened by the mesh itself, where
/// they run nearly straight along the image's sides; the mesh is then bent so its sides follow them.
class MeshSnapper {
public:
    /// `mesh` is given in the pixel coordinates of `image`, and so is the result.
    [[nodiscard]] PageMesh snap(const PageMesh& mesh, const cv::Mat& image) const;
};

} // namespace docscan
