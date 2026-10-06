#pragma once

#include "export/PageEncoder.hpp"
#include "pipeline/PipelineObserver.hpp"

#include <filesystem>

namespace docscan {

/// Saves the image after every step: <dir>/<page>/01-crop.jpg, 02-resize.jpg, ...
class DebugWriter final : public PipelineObserver {
public:
    explicit DebugWriter(std::filesystem::path directory);

    void stepFinished(std::string_view page, std::size_t index, const FilterSpec& step,
                      const cv::Mat& result) override;

private:
    std::filesystem::path directory_;
    PageEncoder encoder_;
};

} // namespace docscan
