#include "pipeline/Pipeline.hpp"

namespace docscan {

void Pipeline::append(FilterSpec spec, std::unique_ptr<Filter> filter)
{
    steps_.push_back({std::move(spec), std::move(filter)});
}

cv::Mat Pipeline::run(const cv::Mat& image, std::string_view page, PipelineObserver* observer) const
{
    cv::Mat result = image;
    for (std::size_t i = 0; i < steps_.size(); ++i) {
        result = steps_[i].filter->apply(result);
        if (observer)
            observer->stepFinished(page, i + 1, steps_[i].spec, result);
    }
    return result;
}

} // namespace docscan
