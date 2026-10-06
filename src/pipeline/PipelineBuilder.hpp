#pragma once

#include "core/Error.hpp"
#include "core/FilterSpec.hpp"
#include "filters/FilterRegistry.hpp"
#include "pipeline/Pipeline.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace docscan {

/// Assembles a chain step by step: start from a profile or the user's own list, then skip, add and
/// tweak filters. Every change is checked against the registry right away, so errors point at the
/// option that caused them.
class PipelineBuilder {
public:
    explicit PipelineBuilder(const FilterRegistry& registry);

    /// Replaces the whole chain.
    PipelineBuilder& start(const std::vector<FilterSpec>& chain);
    /// Removes every step of the named filter.
    PipelineBuilder& skip(std::string_view filter);
    /// Appends a step to the end of the chain.
    PipelineBuilder& add(const FilterSpec& spec);
    /// Applies "filter.key=value" (or "filter=value" for the main setting) to every step of that filter.
    PipelineBuilder& set(std::string_view assignment);

    const std::vector<FilterSpec>& chain() const { return chain_; }
    Pipeline build() const;

private:
    UsageError notInChain(const std::string& filter, std::string_view option) const;

    const FilterRegistry& registry_;
    std::vector<FilterSpec> chain_;
};

/// The chain in display form: "crop → resize:max=2400 → light".
std::string describe(const std::vector<FilterSpec>& chain);

} // namespace docscan
