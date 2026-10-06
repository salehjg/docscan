#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace docscan {

/// Turns the command-line arguments (image files and folders of images) into the ordered list of pages.
/// Files keep the order given; the images inside a folder are sorted naturally by name.
/// Throws UsageError for missing paths and folders without images.
std::vector<std::filesystem::path> findImages(const std::vector<std::string>& arguments);

} // namespace docscan
