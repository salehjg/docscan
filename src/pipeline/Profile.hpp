#pragma once

#include "core/FilterSpec.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace docscan {

inline constexpr std::string_view kDefaultProfile = "color";

/// A ready-made chain of filters, plus the export settings that suit it.
struct Profile {
    std::string name;
    std::string summary;
    std::vector<FilterSpec> chain;
    int quality = 80; ///< JPEG quality of the pages in the PDF
    std::vector<std::string> aliases = {};
};

/// The profiles users can pick with -p.
class ProfileRegistry {
public:
    void add(Profile profile);

    /// Looks a profile up by name or alias; throws UsageError with a suggestion when unknown.
    const Profile& find(std::string_view name) const;
    const std::vector<Profile>& all() const { return profiles_; }

    /// The profiles that ship with docscan.
    static const ProfileRegistry& builtin();

private:
    std::vector<Profile> profiles_;
};

} // namespace docscan
