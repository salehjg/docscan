#pragma once

#include "core/Settings.hpp"

#include <string>
#include <string_view>

namespace docscan {

/// One step of a chain as the user writes it: "name[:key=value,...]".
/// A value without a key ("rotate:90") is stored under an empty key and means the filter's main setting.
struct FilterSpec {
    std::string name;
    Arguments args;

    /// Parses "resize:max=1600,scale=0.5" or "rotate:90". Throws UsageError when malformed.
    static FilterSpec parse(std::string_view text);

    /// Sets an argument, replacing an earlier value for the same key.
    void set(std::string key, std::string value);

    /// The spec in the same syntax parse() accepts.
    std::string str() const;

    bool operator==(const FilterSpec&) const = default;
};

} // namespace docscan
