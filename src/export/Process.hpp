#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace docscan {

/// Runs external programs directly, without a shell in between.
class Process {
public:
    /// Looks `program` up in $PATH.
    static std::optional<std::filesystem::path> find(std::string_view program);

    /// Runs `program` with `arguments` inside `workDir` with its output discarded,
    /// waits for it and returns its exit status.
    static int run(const std::filesystem::path& program, const std::vector<std::string>& arguments,
                   const std::filesystem::path& workDir);
};

} // namespace docscan
