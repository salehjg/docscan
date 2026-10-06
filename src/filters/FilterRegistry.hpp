#pragma once

#include "core/FilterSpec.hpp"
#include "filters/Filter.hpp"

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace docscan {

/// Knows every filter by name and creates configured instances from specs.
class FilterRegistry {
public:
    using Factory = std::function<std::unique_ptr<Filter>(const Settings&)>;

    template <std::derived_from<Filter> T>
    void add()
    {
        add(T::info(), [](const Settings& settings) { return std::make_unique<T>(settings); });
    }
    void add(FilterInfo info, Factory factory);

    /// Looks a filter up by name or alias; throws UsageError with a suggestion when unknown.
    const FilterInfo& find(std::string_view name) const;

    /// The canonical form of `spec`: real filter name, bare value keyed by the main setting,
    /// every value checked. Throws UsageError when something is wrong.
    FilterSpec resolve(const FilterSpec& spec) const;

    std::unique_ptr<Filter> create(const FilterSpec& spec) const;

    std::vector<const FilterInfo*> all() const;
    std::vector<std::string> names() const;

    /// The filters that ship with docscan.
    static const FilterRegistry& builtin();

private:
    struct Entry {
        FilterInfo info;
        Factory factory;
    };

    const Entry& entry(std::string_view name) const;

    std::vector<Entry> entries_;
};

} // namespace docscan
