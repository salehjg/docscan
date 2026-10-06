#pragma once

#include "core/Settings.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace docscan {

/// What docscan knows about a filter: its name, what it does and which settings it takes.
/// This single description drives validation, defaults, `--filters` and `--dry-run`.
struct FilterInfo {
    std::string name;
    std::string summary;
    std::vector<ParamSpec> params; ///< The first one is the main setting, set by a bare value ("rotate:90").
    std::vector<std::string> aliases = {};
};

/// One image-processing step. A filter is configured once in its constructor and is immutable
/// afterwards, so a single instance can process many pages concurrently.
///
/// To add a filter: derive from Filter, give the class `static FilterInfo info()` and a constructor
/// taking `const Settings&`, and register it in FilterRegistry::builtin().
class Filter {
public:
    Filter() = default;
    Filter(const Filter&) = delete;
    Filter& operator=(const Filter&) = delete;
    virtual ~Filter() = default;

    /// Takes an 8-bit image with 1 or 3 channels and returns the result without modifying the input.
    [[nodiscard]] virtual cv::Mat apply(const cv::Mat& image) const = 0;
};

} // namespace docscan
