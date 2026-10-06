#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace docscan::text {

/// Number of single-character edits that turn `a` into `b` (Levenshtein distance).
std::size_t editDistance(std::string_view a, std::string_view b);

/// " (did you mean 'x'?)" for the candidate closest to `word`, or "" when none is close enough.
std::string suggestion(std::string_view word, const std::vector<std::string>& candidates);

/// Orders strings the way people expect numbers to sort: "page2" before "page10".
bool naturalLess(std::string_view a, std::string_view b);

std::string join(const std::vector<std::string>& parts, std::string_view separator);

} // namespace docscan::text
