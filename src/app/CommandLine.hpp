#pragma once

#include "app/Options.hpp"

#include <expected>

namespace docscan {

/// Defines docscan's options and help text, and parses argv into Options.
class CommandLine {
public:
    /// The parsed options, or the exit code when parsing already ended the run
    /// (help or version printed, or a usage error reported).
    static std::expected<Options, int> parse(int argc, char** argv);
};

} // namespace docscan
