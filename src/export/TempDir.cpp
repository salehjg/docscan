#include "export/TempDir.hpp"

#include "core/Error.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <system_error>

namespace docscan {

TempDir::TempDir()
{
    std::string pattern = (std::filesystem::temp_directory_path() / "docscan-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr)
        throw ProcessingError(std::format("cannot create a temporary directory: {}", std::strerror(errno)));
    path_ = pattern;
}

TempDir::~TempDir()
{
    if (keep_)
        return;
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
}

} // namespace docscan
