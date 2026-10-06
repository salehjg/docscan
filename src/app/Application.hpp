#pragma once

#include "app/Options.hpp"
#include "pipeline/PipelineBuilder.hpp"
#include "pipeline/Profile.hpp"

namespace docscan {

/// The docscan program: reads the command line, assembles the chain and scans.
class Application {
public:
    /// Runs docscan and returns the process exit code: 0 success, 1 failure, 2 bad usage.
    int run(int argc, char** argv);

private:
    void execute(const Options& options);
    PipelineBuilder chainFor(const Profile& profile, const Options& options) const;
    void scan(const Options& options, const Profile& profile, const Pipeline& pipeline) const;

    static void printFilters();
    static void printProfiles();
    static void printPlan(const Options& options, const Profile& profile, const std::vector<FilterSpec>& chain);
};

} // namespace docscan
