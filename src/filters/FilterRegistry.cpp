#include "filters/FilterRegistry.hpp"

#include "core/Error.hpp"
#include "core/Text.hpp"
#include "filters/BwFilter.hpp"
#include "filters/ContrastFilter.hpp"
#include "filters/CropFilter.hpp"
#include "filters/DenoiseFilter.hpp"
#include "filters/GrayFilter.hpp"
#include "filters/LightFilter.hpp"
#include "filters/ResizeFilter.hpp"
#include "filters/RotateFilter.hpp"
#include "filters/SharpenFilter.hpp"

#include <algorithm>
#include <format>

namespace docscan {

void FilterRegistry::add(FilterInfo info, Factory factory)
{
    entries_.push_back({std::move(info), std::move(factory)});
}

const FilterInfo& FilterRegistry::find(std::string_view name) const { return entry(name).info; }

FilterSpec FilterRegistry::resolve(const FilterSpec& spec) const
{
    const FilterInfo& info = find(spec.name);
    FilterSpec canonical{info.name, {}};
    for (const auto& [key, value] : spec.args) {
        if (!key.empty())
            canonical.set(key, value);
        else if (!info.params.empty())
            canonical.set(info.params.front().name, value);
        else
            throw UsageError(std::format("{} has no settings", info.name));
    }
    Settings{info.name, info.params, canonical.args}; // throws UsageError on bad keys or values
    return canonical;
}

std::unique_ptr<Filter> FilterRegistry::create(const FilterSpec& spec) const
{
    const FilterSpec canonical = resolve(spec);
    const Entry& found = entry(canonical.name);
    return found.factory(Settings(found.info.name, found.info.params, canonical.args));
}

std::vector<const FilterInfo*> FilterRegistry::all() const
{
    std::vector<const FilterInfo*> infos;
    for (const auto& e : entries_)
        infos.push_back(&e.info);
    return infos;
}

std::vector<std::string> FilterRegistry::names() const
{
    std::vector<std::string> names;
    for (const auto& e : entries_)
        names.push_back(e.info.name);
    return names;
}

const FilterRegistry::Entry& FilterRegistry::entry(std::string_view name) const
{
    for (const auto& e : entries_) {
        if (e.info.name == name || std::ranges::contains(e.info.aliases, name))
            return e;
    }
    throw UsageError(
        std::format("unknown filter '{}'{}; see docscan --filters", name, text::suggestion(name, names())));
}

const FilterRegistry& FilterRegistry::builtin()
{
    static const FilterRegistry registry = [] {
        FilterRegistry r;
        r.add<CropFilter>();
        r.add<LightFilter>();
        r.add<ContrastFilter>();
        r.add<GrayFilter>();
        r.add<BwFilter>();
        r.add<ResizeFilter>();
        r.add<RotateFilter>();
        r.add<SharpenFilter>();
        r.add<DenoiseFilter>();
        return r;
    }();
    return registry;
}

} // namespace docscan
