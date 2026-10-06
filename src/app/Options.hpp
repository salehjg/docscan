#pragma once

#include <string>
#include <vector>

namespace docscan {

/// Everything the user asked for on the command line.
struct Options {
    std::vector<std::string> inputs;
    std::string output = "scan.pdf";
    std::string profile;               ///< -p; empty when not given (then the default profile applies)
    std::vector<std::string> filters;  ///< -f: the user's own chain, in order
    std::vector<std::string> added;    ///< -a: filters appended to the chain
    std::vector<std::string> settings; ///< -s: FILTER.KEY=VALUE changes
    std::vector<std::string> skipped;  ///< -x: filters removed from the chain
    std::string page = "a4";
    int quality = 0; ///< 0 = the profile's quality
    std::string imagesDir;
    std::string debugDir;
    unsigned jobs = 1;
    bool dryRun = false;
    bool listFilters = false;
    bool listProfiles = false;
    bool verbose = false;
    bool quiet = false;
};

} // namespace docscan
