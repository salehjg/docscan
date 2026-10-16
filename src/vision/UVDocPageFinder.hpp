#pragma once

#include "ml/UVDocNet.hpp"
#include "vision/MeshSnapper.hpp"
#include "vision/PageMesh.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace docscan {

/// Finds the page, and how it is curled or folded, with the UVDoc network; then moves the mesh's outline onto
/// the paper's edges where the photo shows them.
class UVDocPageFinder {
public:
    /// Loads the network's weights; throws std::runtime_error when `checkpoint` cannot be read.
    explicit UVDocPageFinder(const std::filesystem::path& checkpoint);

    /// Where the weights are: `setting` when not empty, otherwise the first that exists of
    /// $XDG_DATA_HOME/docscan/uvdoc.pkl (~/.local/share by default) and the installed <datadir>/docscan/uvdoc.pkl.
    static std::optional<std::filesystem::path> locate(const std::string& setting);

    /// Whether the crop filter may download() missing weights. Off unless the application turns it on, so tests
    /// and other users of the library never touch the network.
    static void allowDownload(bool allowed);
    static bool downloadAllowed();

    /// Downloads the weights, checks their SHA-256 and saves them where locate() looks first
    /// ($XDG_DATA_HOME/docscan/uvdoc.pkl); returns that path. Throws Error when that fails.
    static std::filesystem::path download();

    /// The mesh over the page in `image`, or nothing when the network's answer is not a plausible page.
    [[nodiscard]] std::optional<PageMesh> find(const cv::Mat& image) const;

private:
    UVDocNet net_;
    MeshSnapper snapper_;
    mutable std::mutex mutex_; ///< one page at a time: each one already keeps every core busy
};

} // namespace docscan
