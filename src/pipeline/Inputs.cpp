#include "pipeline/Inputs.hpp"

#include "core/Error.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <string_view>

namespace docscan {

namespace {

constexpr std::array<std::string_view, 8> kImageExtensions{".jpg", ".jpeg", ".png", ".tif",
                                                           ".tiff", ".webp", ".bmp", ".jp2"};

bool isImage(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
    return std::ranges::contains(kImageExtensions, extension);
}

std::vector<std::filesystem::path> imagesIn(const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> images;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_regular_file() && isImage(entry.path()))
            images.push_back(entry.path());
    }
    if (images.empty())
        throw UsageError(std::format("no images in folder '{}'", folder.string()));
    std::ranges::sort(images, [](const auto& a, const auto& b) {
        return text::naturalLess(a.filename().string(), b.filename().string());
    });
    return images;
}

} // namespace

std::vector<std::filesystem::path> findImages(const std::vector<std::string>& arguments)
{
    std::vector<std::filesystem::path> images;
    for (const auto& argument : arguments) {
        const std::filesystem::path path(argument);
        if (std::filesystem::is_directory(path))
            std::ranges::copy(imagesIn(path), std::back_inserter(images));
        else if (std::filesystem::exists(path))
            images.push_back(path);
        else
            throw UsageError(std::format("'{}' does not exist", argument));
    }
    return images;
}

} // namespace docscan
