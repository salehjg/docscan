#include "core/Text.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <numeric>

namespace docscan::text {

namespace {

bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

/// Length of the run of digits starting at `from`.
std::size_t digitRun(std::string_view s, std::size_t from)
{
    std::size_t end = from;
    while (end < s.size() && isDigit(s[end]))
        ++end;
    return end - from;
}

std::string_view withoutLeadingZeros(std::string_view number)
{
    const auto first = number.find_first_not_of('0');
    return first == std::string_view::npos ? std::string_view{} : number.substr(first);
}

} // namespace

std::size_t editDistance(std::string_view a, std::string_view b)
{
    std::vector<std::size_t> row(b.size() + 1);
    std::iota(row.begin(), row.end(), std::size_t{0});
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}

std::string suggestion(std::string_view word, const std::vector<std::string>& candidates)
{
    const std::string* best = nullptr;
    std::size_t bestDistance = std::max<std::size_t>(2, word.size() / 3) + 1;
    for (const auto& candidate : candidates) {
        if (const auto distance = editDistance(word, candidate); distance < bestDistance) {
            best = &candidate;
            bestDistance = distance;
        }
    }
    return best ? std::format(" (did you mean '{}'?)", *best) : std::string{};
}

bool naturalLess(std::string_view a, std::string_view b)
{
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size()) {
        if (isDigit(a[i]) && isDigit(b[j])) {
            const std::size_t lengthA = digitRun(a, i);
            const std::size_t lengthB = digitRun(b, j);
            const auto numberA = withoutLeadingZeros(a.substr(i, lengthA));
            const auto numberB = withoutLeadingZeros(b.substr(j, lengthB));
            if (numberA.size() != numberB.size())
                return numberA.size() < numberB.size();
            if (numberA != numberB)
                return numberA < numberB;
            i += lengthA;
            j += lengthB;
        } else {
            if (a[i] != b[j])
                return a[i] < b[j];
            ++i;
            ++j;
        }
    }
    return a.size() - i < b.size() - j;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator)
{
    std::string joined;
    for (const auto& part : parts) {
        if (!joined.empty())
            joined += separator;
        joined += part;
    }
    return joined;
}

} // namespace docscan::text
