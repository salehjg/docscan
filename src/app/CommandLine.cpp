#include "app/CommandLine.hpp"

#include "Version.hpp"
#include "core/Text.hpp"
#include "export/PaperSize.hpp"
#include "filters/FilterRegistry.hpp"
#include "pipeline/Profile.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <format>
#include <thread>

namespace docscan {

namespace {

constexpr std::string_view kChain = "Chain (applied in this order: -p or -f, then -x, -a, -s)";
constexpr std::string_view kOutput = "Output";
constexpr std::string_view kInfo = "Information";

unsigned defaultJobs() { return std::clamp(std::thread::hardware_concurrency(), 1u, 4u); }

std::string footer()
{
    std::vector<std::string> profiles;
    for (const auto& profile : ProfileRegistry::builtin().all())
        profiles.push_back(profile.name == kDefaultProfile ? profile.name + " (default)" : profile.name);

    return std::format(R"(
Profiles: {}    (details: docscan --profiles)
Filters:  {}    (details: docscan --filters)
SPEC:     NAME[:KEY=VALUE,...], or NAME:VALUE for the main setting: resize:max=1600, rotate:90

Examples:
  docscan photos/ -o notes.pdf                            color PDF of a folder
  docscan IMG_*.jpg -p bw                                 black & white
  docscan IMG_*.jpg -p gray -s resize.max=1600            gray, smaller pages
  docscan IMG_*.jpg -p bw -a rotate:90 -x light           tweak a profile
  docscan IMG_*.jpg -f crop -f gray -f resize:1200 -f bw  your own chain, in order
  docscan IMG_*.jpg -p bw -n                              show the chain only
)",
                       text::join(profiles, ", "), text::join(FilterRegistry::builtin().names(), " "));
}

} // namespace

std::expected<Options, int> CommandLine::parse(int argc, char** argv)
{
    Options options;
    options.jobs = defaultJobs();

    CLI::App app{"Turn photos of documents into clean, straight PDF pages.", "docscan"};
    app.set_version_flag("-V,--version", std::string(kVersion));
    app.footer(footer());
    const auto formatter = app.get_formatter();
    formatter->column_width(36);
    formatter->long_option_alignment_ratio(1 / 6.f);
    formatter->enable_footer_formatting(false);

    app.add_option("photos", options.inputs, "photos to scan, or folders of photos; pages follow this order")
        ->type_name("PHOTO|FOLDER");

    app.add_option("-p,--profile", options.profile, "starting chain: color (default), gray, bw or photo")
        ->type_name("NAME")
        ->group(std::string(kChain));
    app.add_option("-f,--filter", options.filters, "build your own chain instead of -p; repeat, runs in order")
        ->type_name("SPEC")
        ->allow_extra_args(false)
        ->group(std::string(kChain));
    app.add_option("-x,--skip", options.skipped, "remove a filter from the chain")
        ->type_name("NAME")
        ->allow_extra_args(false)
        ->group(std::string(kChain));
    app.add_option("-a,--add", options.added, "add a filter at the end of the chain")
        ->type_name("SPEC")
        ->allow_extra_args(false)
        ->group(std::string(kChain));
    app.add_option("-s,--set", options.settings, "change a filter's setting, e.g. resize.max=1600")
        ->type_name("NAME.KEY=VALUE")
        ->allow_extra_args(false)
        ->group(std::string(kChain));

    app.add_option("-o,--output", options.output, "PDF file to write")
        ->type_name("FILE")
        ->capture_default_str()
        ->group(std::string(kOutput));
    app.add_option("--page", options.page, "page size: a4, a5, letter, legal, or fit (the photo's own shape)")
        ->type_name("SIZE")
        ->check(CLI::IsMember(PaperSize::names()).description(""))
        ->capture_default_str()
        ->group(std::string(kOutput));
    app.add_option("--quality", options.quality, "JPEG quality 1-100 (default: the profile's)")
        ->type_name("N")
        ->check(CLI::Range(1, 100).description(""))
        ->group(std::string(kOutput));
    app.add_option("--images", options.imagesDir, "also save the finished pages as images in DIR")
        ->type_name("DIR")
        ->group(std::string(kOutput));
    app.add_option("--debug", options.debugDir, "save the image after every filter, per page, in DIR")
        ->type_name("DIR")
        ->group(std::string(kOutput));
    app.add_option("-j,--jobs", options.jobs, "pages processed at the same time")
        ->type_name("N")
        ->check(CLI::PositiveNumber.description(""))
        ->capture_default_str()
        ->group(std::string(kOutput));

    app.add_flag("-n,--dry-run", options.dryRun, "print the chain that would run, then stop")
        ->group(std::string(kInfo));
    app.add_flag("--filters", options.listFilters, "list filters and their settings")->group(std::string(kInfo));
    app.add_flag("--profiles", options.listProfiles, "list profiles and their chains")->group(std::string(kInfo));
    app.add_flag("-v,--verbose", options.verbose, "tell more about each page")->group(std::string(kInfo));
    app.add_flag("-q,--quiet", options.quiet, "print errors only")->group(std::string(kInfo));

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& error) {
        const int code = app.exit(error);
        return std::unexpected(code == 0 ? 0 : 2);
    }
    return options;
}

} // namespace docscan
