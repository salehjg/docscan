#include "pipeline/PipelineBuilder.hpp"

#include "core/Error.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <format>

namespace docscan {

PipelineBuilder::PipelineBuilder(const FilterRegistry& registry) : registry_(registry) {}

PipelineBuilder& PipelineBuilder::start(const std::vector<FilterSpec>& chain)
{
    chain_.clear();
    for (const auto& spec : chain)
        add(spec);
    return *this;
}

PipelineBuilder& PipelineBuilder::skip(std::string_view filter)
{
    const std::string& name = registry_.find(filter).name;
    if (std::erase_if(chain_, [&](const FilterSpec& spec) { return spec.name == name; }) == 0)
        throw notInChain(name, "-x");
    return *this;
}

PipelineBuilder& PipelineBuilder::add(const FilterSpec& spec)
{
    chain_.push_back(registry_.resolve(spec));
    return *this;
}

PipelineBuilder& PipelineBuilder::set(std::string_view assignment)
{
    const auto equals = assignment.find('=');
    if (equals == std::string_view::npos || equals == 0 || equals + 1 == assignment.size())
        throw UsageError(std::format("-s '{}': expected FILTER.SETTING=VALUE, like resize.max=1600", assignment));

    const auto target = assignment.substr(0, equals);
    const auto dot = target.find('.');
    const std::string& name = registry_.find(target.substr(0, dot)).name;
    const std::string key = dot == std::string_view::npos ? "" : std::string(target.substr(dot + 1));
    const std::string value(assignment.substr(equals + 1));

    bool found = false;
    for (auto& spec : chain_) {
        if (spec.name != name)
            continue;
        FilterSpec changed = spec;
        changed.set(key, value);
        spec = registry_.resolve(changed);
        found = true;
    }
    if (!found)
        throw notInChain(name, "-s");
    return *this;
}

Pipeline PipelineBuilder::build() const
{
    Pipeline pipeline;
    for (const auto& spec : chain_)
        pipeline.append(spec, registry_.create(spec));
    return pipeline;
}

UsageError PipelineBuilder::notInChain(const std::string& filter, std::string_view option) const
{
    return UsageError(std::format("{}: there is no {} in the chain ({}); add it with -a {}", option, filter,
                                  chain_.empty() ? "empty" : describe(chain_), filter));
}

std::string describe(const std::vector<FilterSpec>& chain)
{
    std::vector<std::string> steps;
    for (const auto& spec : chain)
        steps.push_back(spec.str());
    return text::join(steps, " → ");
}

} // namespace docscan
