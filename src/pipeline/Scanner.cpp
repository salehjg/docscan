#include "pipeline/Scanner.hpp"

#include "core/Error.hpp"
#include "core/Log.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <atomic>
#include <exception>
#include <format>
#include <mutex>
#include <thread>

namespace docscan {

Scanner::Scanner(const Pipeline& pipeline, unsigned jobs, PipelineObserver* observer)
    : pipeline_(pipeline), jobs_(std::max(1u, jobs)), observer_(observer)
{
}

void Scanner::run(const std::vector<std::filesystem::path>& images,
                  const std::vector<std::unique_ptr<Exporter>>& exporters) const
{
    for (const auto& exporter : exporters)
        exporter->begin(images.size());

    std::atomic<std::size_t> next{0};
    std::atomic<std::size_t> done{0};
    std::exception_ptr failure;
    std::mutex failureMutex;

    const auto work = [&] {
        for (std::size_t i = next++; i < images.size(); i = next++) {
            try {
                const cv::Mat page = scan(images[i], i);
                for (const auto& exporter : exporters)
                    exporter->addPage(i, page);
                Log::info(std::format("[{}/{}] {}", ++done, images.size(), images[i].filename().string()));
            } catch (...) {
                const std::scoped_lock lock(failureMutex);
                if (!failure)
                    failure = std::current_exception();
                next = images.size(); // hand out no more pages
            }
        }
    };

    const auto workers = std::min<std::size_t>(jobs_, images.size());
    if (workers > 1)
        cv::setNumThreads(1); // pages already run in parallel; nested OpenCV threads would only compete
    {
        std::vector<std::jthread> threads;
        for (std::size_t t = 0; t < workers; ++t)
            threads.emplace_back(work);
    }
    if (failure)
        std::rethrow_exception(failure);

    for (const auto& exporter : exporters)
        exporter->finish();
}

cv::Mat Scanner::scan(const std::filesystem::path& image, std::size_t index) const
{
    const Log::PageScope scope(image.filename().string());
    const cv::Mat photo = cv::imread(image.string(), cv::IMREAD_COLOR);
    if (photo.empty())
        throw ProcessingError(std::format("cannot read '{}' as an image", image.string()));
    return pipeline_.run(photo, std::format("{:03}-{}", index + 1, image.stem().string()), observer_);
}

} // namespace docscan
