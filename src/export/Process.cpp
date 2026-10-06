#include "export/Process.hpp"

#include "core/Error.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <format>
#include <ranges>

namespace docscan {

std::optional<std::filesystem::path> Process::find(std::string_view program)
{
    const char* path = std::getenv("PATH");
    if (path == nullptr)
        return std::nullopt;
    for (const auto entry : std::views::split(std::string_view(path), ':')) {
        const std::filesystem::path directory(std::string_view(entry.begin(), entry.end()));
        const std::filesystem::path candidate = directory / program;
        if (::access(candidate.c_str(), X_OK) == 0)
            return candidate;
    }
    return std::nullopt;
}

int Process::run(const std::filesystem::path& program, const std::vector<std::string>& arguments,
                 const std::filesystem::path& workDir)
{
    // Everything the child needs is prepared before fork(): after it, only async-signal-safe calls are allowed.
    std::vector<std::string> strings{program.string()};
    strings.insert(strings.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (auto& s : strings)
        argv.push_back(s.data());
    argv.push_back(nullptr);
    const std::string directory = workDir.string();

    const pid_t pid = ::fork();
    if (pid < 0)
        throw ProcessingError(std::format("cannot start {}: {}", program.string(), std::strerror(errno)));
    if (pid == 0) {
        if (::chdir(directory.c_str()) != 0)
            ::_exit(127);
        if (const int devNull = ::open("/dev/null", O_RDWR); devNull >= 0) {
            ::dup2(devNull, STDIN_FILENO);
            ::dup2(devNull, STDOUT_FILENO);
            ::dup2(devNull, STDERR_FILENO);
        }
        ::execv(argv[0], argv.data());
        ::_exit(127);
    }

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            throw ProcessingError(std::format("lost track of {}: {}", program.string(), std::strerror(errno)));
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace docscan
