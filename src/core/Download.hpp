#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace docscan {

/// Fetches files over the network with libcurl.
class Download {
public:
    /// Saves `url` as `target`, creating its folder, if the file's SHA-256 is `sha256` (lowercase hex).
    /// `target` appears complete or not at all; throws ProcessingError when the download fails or the checksum
    /// differs.
    static void fetch(const std::string& url, const std::filesystem::path& target, std::string_view sha256);
};

} // namespace docscan
