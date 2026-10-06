#pragma once

#include "core/FilterSpec.hpp"

#include <opencv2/core.hpp>

#include <cstddef>
#include <string_view>

namespace docscan {

/// Gets to see every intermediate image a pipeline produces.
class PipelineObserver {
public:
    virtual ~PipelineObserver() = default;

    /// Called after step `index` (1-based) finished on `page`. Called from several threads at once.
    virtual void stepFinished(std::string_view page, std::size_t index, const FilterSpec& step,
                              const cv::Mat& result) = 0;
};

} // namespace docscan
