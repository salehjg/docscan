#include "core/Download.hpp"
#include "core/Error.hpp"
#include "export/TempDir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

using namespace docscan;

namespace {

constexpr const char* kAbcSha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"; // of "abc"

std::filesystem::path writeFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream(path, std::ios::binary) << content;
    return path;
}

std::string readFile(const std::filesystem::path& path)
{
    std::ostringstream content;
    content << std::ifstream(path, std::ios::binary).rdbuf();
    return content.str();
}

std::string fileUrl(const std::filesystem::path& path) { return "file://" + path.string(); }

std::ptrdiff_t entries(const std::filesystem::path& folder)
{
    return std::distance(std::filesystem::directory_iterator(folder), std::filesystem::directory_iterator());
}

} // namespace

TEST_CASE("a download is saved, with its folder, when its checksum matches")
{
    const TempDir dir;
    const auto source = writeFile(dir.path() / "source", "abc");
    const auto target = dir.path() / "data/docscan/model.pkl";
    Download::fetch(fileUrl(source), target, kAbcSha256);
    CHECK(readFile(target) == "abc");
    CHECK(entries(target.parent_path()) == 1); // nothing left over beside it
}

TEST_CASE("a download with the wrong checksum is thrown away")
{
    const TempDir dir;
    const auto source = writeFile(dir.path() / "source", "abd");
    const auto target = dir.path() / "data/model.pkl";
    CHECK_THROWS_AS(Download::fetch(fileUrl(source), target, kAbcSha256), ProcessingError);
    CHECK(entries(target.parent_path()) == 0);
}

TEST_CASE("a failed download leaves nothing behind")
{
    const TempDir dir;
    CHECK_THROWS_AS(Download::fetch(fileUrl(dir.path() / "missing"), dir.path() / "model.pkl", kAbcSha256),
                    ProcessingError);
    CHECK(entries(dir.path()) == 0);
}
