#include "pipeline/DebugWriter.hpp"

#include <format>

namespace docscan {

namespace {

constexpr int kQuality = 90;

} // namespace

DebugWriter::DebugWriter(std::filesystem::path directory) : directory_(std::move(directory)), encoder_(kQuality) {}

void DebugWriter::stepFinished(std::string_view page, std::size_t index, const FilterSpec& step, const cv::Mat& result)
{
    const auto pageDir = directory_ / page;
    std::filesystem::create_directories(pageDir);
    encoder_.write(result, pageDir / std::format("{:02}-{}", index, step.name));
}

} // namespace docscan
