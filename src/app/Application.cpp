#include "app/Application.hpp"

#include "app/CommandLine.hpp"
#include "core/Error.hpp"
#include "core/Log.hpp"
#include "core/Text.hpp"
#include "export/ImageExporter.hpp"
#include "export/PdfExporter.hpp"
#include "pipeline/DebugWriter.hpp"
#include "pipeline/Inputs.hpp"
#include "pipeline/Scanner.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <optional>
#include <print>

namespace docscan {

namespace {

int qualityFor(const Options& options, const Profile& profile)
{
    return options.quality > 0 ? options.quality : profile.quality;
}

/// -f and -p together: -f would silently replace the profile's chain, so explain what to use instead,
/// guessing from the first -f filter (a step of the profile to change, or a new one to add).
UsageError ownChainWithProfile(const Profile& profile, const std::string& filter)
{
    const auto& registry = FilterRegistry::builtin();
    const FilterSpec spec = registry.resolve(FilterSpec::parse(filter));
    const bool inProfile = std::ranges::any_of(
        profile.chain, [&](const FilterSpec& step) { return registry.find(step.name).name == spec.name; });

    std::string example;
    if (!inProfile)
        example = std::format(", like -a {}", spec.str());
    else if (!spec.args.empty())
        example = std::format(", like -s {}.{}={}", spec.name, spec.args.front().first, spec.args.front().second);
    return UsageError(std::format("-f builds a chain from scratch and can't be combined with -p {0}; to adjust {0}, "
                                  "change a step with -s, add one with -a or remove one with -x{1}",
                                  profile.name, example));
}

} // namespace

int Application::run(int argc, char** argv)
{
    const auto options = CommandLine::parse(argc, argv);
    if (!options)
        return options.error();

    try {
        execute(*options);
        return 0;
    } catch (const UsageError& error) {
        std::println(stderr, "docscan: {}", error.what());
        return 2;
    } catch (const std::exception& error) {
        std::println(stderr, "docscan: error: {}", error.what());
        return 1;
    }
}

void Application::execute(const Options& options)
{
    Log::setLevel(options.quiet     ? Log::Level::Quiet
                  : options.verbose ? Log::Level::Verbose
                                    : Log::Level::Normal);
    if (options.listFilters) {
        printFilters();
        return;
    }
    if (options.listProfiles) {
        printProfiles();
        return;
    }

    const Profile& profile = ProfileRegistry::builtin().find(options.profile.empty() ? kDefaultProfile
                                                                                     : options.profile);
    const PipelineBuilder builder = chainFor(profile, options);
    const Pipeline pipeline = builder.build(); // creates every filter, so bad settings fail before any work
    if (options.dryRun) {
        printPlan(options, profile, builder.chain());
        return;
    }
    if (options.inputs.empty())
        throw UsageError("no photos given; try: docscan photos/ -o scan.pdf (or docscan -h)");

    Log::detail(std::format("chain: {}", describe(builder.chain())));
    scan(options, profile, pipeline);
}

PipelineBuilder Application::chainFor(const Profile& profile, const Options& options) const
{
    PipelineBuilder builder(FilterRegistry::builtin());
    if (options.filters.empty()) {
        builder.start(profile.chain);
    } else if (!options.profile.empty()) {
        throw ownChainWithProfile(profile, options.filters.front());
    } else {
        std::vector<FilterSpec> own;
        for (const auto& spec : options.filters)
            own.push_back(FilterSpec::parse(spec));
        builder.start(own);
    }
    for (const auto& name : options.skipped)
        builder.skip(name);
    for (const auto& spec : options.added)
        builder.add(FilterSpec::parse(spec));
    for (const auto& assignment : options.settings)
        builder.set(assignment);
    return builder;
}

void Application::scan(const Options& options, const Profile& profile, const Pipeline& pipeline) const
{
    const auto images = findImages(options.inputs);
    const std::filesystem::path output(options.output);
    if (const auto folder = output.parent_path(); !folder.empty() && !std::filesystem::is_directory(folder))
        throw UsageError(std::format("cannot write '{}': folder '{}' does not exist", options.output, folder.string()));

    const int quality = qualityFor(options, profile);
    std::vector<std::unique_ptr<Exporter>> exporters;
    exporters.push_back(std::make_unique<PdfExporter>(output, PaperSize::fromName(options.page), quality));
    if (!options.imagesDir.empty())
        exporters.push_back(std::make_unique<ImageExporter>(options.imagesDir, quality));

    std::optional<DebugWriter> debug;
    if (!options.debugDir.empty())
        debug.emplace(options.debugDir);

    Scanner(pipeline, options.jobs, debug ? &*debug : nullptr).run(images, exporters);
    Log::info(std::format("wrote {} ({} page{})", output.string(), images.size(), images.size() == 1 ? "" : "s"));
}

void Application::printFilters()
{
    std::println("Filters, used as -f NAME[:KEY=VALUE,...]; a bare value sets the first setting (rotate:90).");
    std::println("Each setting is shown with its default.\n");
    for (const FilterInfo* info : FilterRegistry::builtin().all()) {
        std::string title = info->name;
        if (!info->aliases.empty())
            title += std::format(" ({})", text::join(info->aliases, ", "));
        std::println("{:<12}{}", title, info->summary);
        for (const auto& param : info->params)
            std::println("    {:<9}{:<9}{}", param.name, param.defaultValue, param.help);
        std::println("");
    }
}

void Application::printProfiles()
{
    std::println("Profiles, used as -p NAME; change them with -s, -x and -a (see docscan -h).\n");
    const auto& registry = FilterRegistry::builtin();
    for (const auto& profile : ProfileRegistry::builtin().all()) {
        std::vector<FilterSpec> chain;
        for (const auto& spec : profile.chain)
            chain.push_back(registry.resolve(spec));
        const std::string title = profile.name == kDefaultProfile ? profile.name + " (default)" : profile.name;
        std::println("{:<16}{}", title, profile.summary);
        std::println("{:<16}{}\n", "", describe(chain));
    }
}

void Application::printPlan(const Options& options, const Profile& profile, const std::vector<FilterSpec>& chain)
{
    std::vector<std::string> flags;
    for (const auto& spec : chain)
        flags.push_back("-f " + spec.str());
    std::println("profile  {}", options.filters.empty() ? profile.name : "none, own chain from -f");
    std::println("chain    {}", chain.empty() ? "(empty)" : describe(chain));
    std::println("as -f    {}", text::join(flags, " "));
    std::println("output   {} (page {}, JPEG quality {})", options.output, options.page, qualityFor(options, profile));
}

} // namespace docscan
