#include "core/Settings.hpp"

#include "core/Error.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <utility>

namespace docscan {

namespace {

template <class T>
bool parseNumber(std::string_view text, T& value)
{
    const auto* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end;
}

void checkRange(std::string_view owner, const ParamSpec& spec, double value)
{
    if (value >= spec.min && value <= spec.max)
        return;
    const auto where = std::format("{}.{}", owner, spec.name);
    if (std::isinf(spec.max))
        throw UsageError(std::format("{}: must be at least {}, got {}", where, spec.min, value));
    if (std::isinf(spec.min))
        throw UsageError(std::format("{}: must be at most {}, got {}", where, spec.max, value));
    throw UsageError(std::format("{}: must be between {} and {}, got {}", where, spec.min, spec.max, value));
}

Settings::Value parseValue(std::string_view owner, const ParamSpec& spec, std::string_view text)
{
    const auto expected = [&](std::string_view what) {
        return UsageError(std::format("{}.{}: expected {}, got '{}'", owner, spec.name, what, text));
    };

    switch (spec.type) {
    case ParamType::Integer: {
        int value = 0;
        if (!parseNumber(text, value))
            throw expected("a whole number");
        checkRange(owner, spec, value);
        return value;
    }
    case ParamType::Number: {
        double value = 0;
        if (!parseNumber(text, value))
            throw expected("a number");
        checkRange(owner, spec, value);
        return value;
    }
    case ParamType::Boolean:
        if (text == "yes" || text == "true" || text == "on" || text == "1")
            return true;
        if (text == "no" || text == "false" || text == "off" || text == "0")
            return false;
        throw expected("yes or no");
    case ParamType::Choice:
        if (std::ranges::contains(spec.choices, text))
            return std::string(text);
        throw expected(std::format("one of {}", text::join(spec.choices, ", ")));
    case ParamType::Text:
        return std::string(text);
    }
    std::unreachable();
}

UsageError unknownSetting(std::string_view owner, std::string_view key, std::span<const ParamSpec> specs)
{
    if (specs.empty())
        return UsageError(std::format("{} has no settings", owner));
    std::vector<std::string> names;
    for (const auto& spec : specs)
        names.push_back(spec.name);
    return UsageError(std::format("{} has no setting '{}'{}; its settings are: {}", owner, key,
                                  text::suggestion(key, names), text::join(names, ", ")));
}

} // namespace

ParamSpec ParamSpec::integer(std::string name, int defaultValue, int min, int max, std::string help)
{
    return {.name = std::move(name),
            .type = ParamType::Integer,
            .defaultValue = std::to_string(defaultValue),
            .help = std::move(help),
            .min = double(min),
            .max = double(max)};
}

ParamSpec ParamSpec::number(std::string name, double defaultValue, double min, double max, std::string help)
{
    return {.name = std::move(name),
            .type = ParamType::Number,
            .defaultValue = std::format("{}", defaultValue),
            .help = std::move(help),
            .min = min,
            .max = max};
}

ParamSpec ParamSpec::boolean(std::string name, bool defaultValue, std::string help)
{
    return {.name = std::move(name),
            .type = ParamType::Boolean,
            .defaultValue = defaultValue ? "yes" : "no",
            .help = std::move(help)};
}

ParamSpec ParamSpec::choice(std::string name, std::vector<std::string> choices, std::string help)
{
    std::string defaultValue = choices.front();
    return {.name = std::move(name),
            .type = ParamType::Choice,
            .defaultValue = std::move(defaultValue),
            .help = std::move(help),
            .choices = std::move(choices)};
}

ParamSpec ParamSpec::text(std::string name, std::string defaultValue, std::string help)
{
    return {.name = std::move(name),
            .type = ParamType::Text,
            .defaultValue = std::move(defaultValue),
            .help = std::move(help)};
}

Settings::Settings(std::string_view owner, std::span<const ParamSpec> specs, const Arguments& arguments)
{
    for (const auto& spec : specs)
        values_[spec.name] = parseValue(owner, spec, spec.defaultValue);

    for (const auto& [key, value] : arguments) {
        const auto spec = std::ranges::find(specs, key, &ParamSpec::name);
        if (spec == specs.end())
            throw unknownSetting(owner, key, specs);
        values_[key] = parseValue(owner, *spec, value);
    }
}

} // namespace docscan
