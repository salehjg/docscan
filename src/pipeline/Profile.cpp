#include "pipeline/Profile.hpp"

#include "core/Error.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace docscan {

namespace {

/// Parses a space-separated list of filter specs.
std::vector<FilterSpec> chain(std::string_view specs)
{
    std::vector<FilterSpec> steps;
    for (const auto step : std::views::split(specs, ' '))
        steps.push_back(FilterSpec::parse(std::string_view(step.begin(), step.end())));
    return steps;
}

} // namespace

void ProfileRegistry::add(Profile profile) { profiles_.push_back(std::move(profile)); }

const Profile& ProfileRegistry::find(std::string_view name) const
{
    for (const auto& profile : profiles_) {
        if (profile.name == name || std::ranges::contains(profile.aliases, name))
            return profile;
    }
    std::vector<std::string> names;
    for (const auto& profile : profiles_)
        names.push_back(profile.name);
    throw UsageError(std::format("unknown profile '{}'{}; choose one of: {}", name, text::suggestion(name, names),
                                 text::join(names, ", ")));
}

const ProfileRegistry& ProfileRegistry::builtin()
{
    static const ProfileRegistry registry = [] {
        ProfileRegistry r;
        r.add({"color", "clean color pages", chain("crop resize:2400 light contrast:1.3 sharpen"), 80, {"colour"}});
        r.add({"gray", "clean grayscale pages", chain("crop resize:2400 gray light contrast:1.3 sharpen"), 80,
               {"grey"}});
        r.add({"bw", "crisp black & white pages, smallest files", chain("crop resize:3300 gray light bw"), 80});
        r.add({"photo", "only crop and straighten, keep the original look", chain("crop resize:2400"), 85});
        return r;
    }();
    return registry;
}

} // namespace docscan
