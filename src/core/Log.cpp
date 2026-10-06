#include "core/Log.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <print>
#include <utility>

namespace docscan {

namespace {

std::atomic<Log::Level> currentLevel{Log::Level::Normal};
std::mutex outputMutex;
thread_local std::string currentPage;

void write(std::string_view prefix, std::string_view message)
{
    const std::scoped_lock lock(outputMutex);
    if (currentPage.empty())
        std::println(stderr, "{}{}", prefix, message);
    else
        std::println(stderr, "{}{}: {}", prefix, currentPage, message);
}

} // namespace

void Log::setLevel(Level level) { currentLevel = level; }

void Log::info(std::string_view message)
{
    if (currentLevel != Level::Quiet)
        write("", message);
}

void Log::detail(std::string_view message)
{
    if (currentLevel == Level::Verbose)
        write("  ", message);
}

void Log::warn(std::string_view message)
{
    if (currentLevel != Level::Quiet)
        write("warning: ", message);
}

Log::PageScope::PageScope(std::string page) : previous_(std::exchange(currentPage, std::move(page))) {}

Log::PageScope::~PageScope() { currentPage = std::move(previous_); }

} // namespace docscan
