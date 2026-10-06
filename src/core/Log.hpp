#pragma once

#include <string>
#include <string_view>

namespace docscan {

/// Thread-safe messages on stderr, filtered by verbosity.
class Log {
public:
    enum class Level { Quiet, Normal, Verbose };

    static void setLevel(Level level);
    /// Progress and results; hidden by --quiet.
    static void info(std::string_view message);
    /// Extra details; shown only with --verbose.
    static void detail(std::string_view message);
    /// Something the user should know about; hidden by --quiet.
    static void warn(std::string_view message);

    /// While alive, messages logged by the current thread are prefixed with `page`.
    class PageScope {
    public:
        explicit PageScope(std::string page);
        ~PageScope();
        PageScope(const PageScope&) = delete;
        PageScope& operator=(const PageScope&) = delete;

    private:
        std::string previous_;
    };
};

} // namespace docscan
