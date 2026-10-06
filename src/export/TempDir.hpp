#pragma once

#include <filesystem>

namespace docscan {

/// A fresh private directory under $TMPDIR, deleted with everything in it when the object goes away.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

    /// Leaves the directory in place, e.g. so the user can inspect what went wrong.
    void keep() { keep_ = true; }

private:
    std::filesystem::path path_;
    bool keep_ = false;
};

} // namespace docscan
