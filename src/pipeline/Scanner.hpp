#pragma once

#include "export/Exporter.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/PipelineObserver.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace docscan {

/// Runs the pipeline over every photo, several pages at a time, and hands the results to the exporters.
class Scanner {
public:
    Scanner(const Pipeline& pipeline, unsigned jobs, PipelineObserver* observer = nullptr);

    /// Processes `images` and passes page i to every exporter. Rethrows the first error once all workers stopped.
    void run(const std::vector<std::filesystem::path>& images,
             const std::vector<std::unique_ptr<Exporter>>& exporters) const;

private:
    cv::Mat scan(const std::filesystem::path& image, std::size_t index) const;

    const Pipeline& pipeline_;
    unsigned jobs_;
    PipelineObserver* observer_;
};

} // namespace docscan
