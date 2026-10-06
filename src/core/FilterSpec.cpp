#include "core/FilterSpec.hpp"

#include "core/Error.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace docscan {

FilterSpec FilterSpec::parse(std::string_view text)
{
    const auto colon = text.find(':');
    FilterSpec spec{std::string(text.substr(0, colon)), {}};
    if (spec.name.empty())
        throw UsageError(std::format("'{}': missing filter name, expected NAME[:KEY=VALUE,...]", text));
    if (colon == std::string_view::npos)
        return spec;

    const auto list = text.substr(colon + 1);
    if (list.empty())
        throw UsageError(std::format("'{}': nothing after ':'", text));

    for (const auto part : std::views::split(list, ',')) {
        const std::string_view item(part.begin(), part.end());
        const auto equals = item.find('=');
        const auto key = equals == std::string_view::npos ? std::string_view{} : item.substr(0, equals);
        const auto value = equals == std::string_view::npos ? item : item.substr(equals + 1);
        if (value.empty() || (equals != std::string_view::npos && key.empty()))
            throw UsageError(std::format("'{}': expected KEY=VALUE, got '{}'", text, item));
        spec.set(std::string(key), std::string(value));
    }
    return spec;
}

void FilterSpec::set(std::string key, std::string value)
{
    const auto existing = std::ranges::find(args, key, &Arguments::value_type::first);
    if (existing != args.end())
        existing->second = std::move(value);
    else
        args.emplace_back(std::move(key), std::move(value));
}

std::string FilterSpec::str() const
{
    std::string text = name;
    char separator = ':';
    for (const auto& [key, value] : args) {
        text += separator;
        text += key.empty() ? value : key + '=' + value;
        separator = ',';
    }
    return text;
}

} // namespace docscan
