#pragma once

#include <format>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace docscan {

/// Settings as written by the user, in the order given: {"max", "1600"}, ...
using Arguments = std::vector<std::pair<std::string, std::string>>;

enum class ParamType { Integer, Number, Boolean, Choice, Text };

/// Describes one setting of a filter: its type, default, allowed values and help text.
struct ParamSpec {
    std::string name;
    ParamType type = ParamType::Text;
    std::string defaultValue;
    std::string help;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    std::vector<std::string> choices = {};

    static ParamSpec integer(std::string name, int defaultValue, int min, int max, std::string help);
    static ParamSpec number(std::string name, double defaultValue, double min, double max, std::string help);
    static ParamSpec boolean(std::string name, bool defaultValue, std::string help);
    /// The first choice is the default.
    static ParamSpec choice(std::string name, std::vector<std::string> choices, std::string help);
    static ParamSpec text(std::string name, std::string defaultValue, std::string help);
};

/// Checked, typed values of a filter's settings, with defaults filled in.
class Settings {
public:
    using Value = std::variant<bool, int, double, std::string>;

    Settings() = default;

    /// Checks `arguments` against `specs` and fills in the defaults.
    /// Throws UsageError naming `owner` (e.g. "resize.max: ...") for unknown keys or bad values.
    Settings(std::string_view owner, std::span<const ParamSpec> specs, const Arguments& arguments);

    template <class T>
    T get(std::string_view key) const
    {
        const auto it = values_.find(key);
        if (it == values_.end())
            throw std::logic_error(std::format("no setting named '{}'", key));
        return std::get<T>(it->second);
    }

private:
    std::map<std::string, Value, std::less<>> values_;
};

} // namespace docscan
