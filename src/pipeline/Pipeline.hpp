#pragma once

#include "core/FilterSpec.hpp"
#include "filters/Filter.hpp"
#include "pipeline/PipelineObserver.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace docscan {

/// An ordered chain of filters. Running it does not change it, so pages can be processed concurrently.
class Pipeline {
public:
    void append(FilterSpec spec, std::unique_ptr<Filter> filter);

    /// Runs every step in order. `observer`, when given, sees each intermediate result.
    cv::Mat run(const cv::Mat& image, std::string_view page, PipelineObserver* observer = nullptr) const;

    std::size_t size() const { return steps_.size(); }

private:
    struct Step {
        FilterSpec spec;
        std::unique_ptr<Filter> filter;
    };

    std::vector<Step> steps_;
};

} // namespace docscan
