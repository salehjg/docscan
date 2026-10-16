#include "vision/UVDocPageFinder.hpp"

#include "core/Download.hpp"
#include "core/Error.hpp"
#include "core/Log.hpp"

#include <atomic>
#include <cstdlib>
#include <format>
#include <string_view>
#include <vector>

#ifndef DOCSCAN_DATA_DIR
#define DOCSCAN_DATA_DIR "/usr/share/docscan"
#endif

namespace docscan {

namespace {

constexpr const char* kModelFile = "uvdoc.pkl";
// The authors' model/best_model.pkl at the commit the network was checked against; the PKGBUILD pins the same.
constexpr const char* kModelUrl =
    "https://github.com/tanguymagne/UVDoc/raw/4c9b82b537057aff2526e6dd118a847cdd072e82/model/best_model.pkl";
constexpr std::string_view kModelSha256 = "7e90861b8a516eb4bc51f84bd889cb77275743d2d1d3ca8091951ec9f2b7da23";

std::atomic<bool> downloadEnabled{false};

/// $XDG_DATA_HOME/docscan/uvdoc.pkl, ~/.local/share by default; nothing when neither variable is set.
std::optional<std::filesystem::path> userModel()
{
    if (const char* data = std::getenv("XDG_DATA_HOME"); data && *data)
        return std::filesystem::path(data) / "docscan" / kModelFile;
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".local/share/docscan" / kModelFile;
    return std::nullopt;
}

} // namespace

UVDocPageFinder::UVDocPageFinder(const std::filesystem::path& checkpoint) : net_(checkpoint) {}

std::optional<std::filesystem::path> UVDocPageFinder::locate(const std::string& setting)
{
    if (!setting.empty())
        return std::filesystem::path(setting);

    std::vector<std::filesystem::path> candidates;
    if (auto user = userModel())
        candidates.push_back(std::move(*user));
    candidates.push_back(std::filesystem::path(DOCSCAN_DATA_DIR) / kModelFile);

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error))
            return candidate;
    }
    return std::nullopt;
}

void UVDocPageFinder::allowDownload(bool allowed) { downloadEnabled = allowed; }

bool UVDocPageFinder::downloadAllowed() { return downloadEnabled; }

std::filesystem::path UVDocPageFinder::download()
{
    const auto target = userModel();
    if (!target)
        throw ProcessingError("cannot download the UVDoc model: neither $XDG_DATA_HOME nor $HOME is set");
    Log::info(std::format("downloading the UVDoc model (32 MB, only this once) to {}", target->string()));
    Download::fetch(kModelUrl, *target, kModelSha256);
    return *target;
}

std::optional<PageMesh> UVDocPageFinder::find(const cv::Mat& image) const
{
    cv::Mat grid;
    {
        const std::scoped_lock lock(mutex_);
        grid = net_.predictGrid(image);
    }
    const PageMesh mesh(std::move(grid));
    if (!mesh.plausible(image.size()))
        return std::nullopt;
    return snapper_.snap(mesh, image);
}

} // namespace docscan
