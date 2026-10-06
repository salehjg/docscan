#include "core/Log.hpp"
#include "filters/CropFilter.hpp"

#include <opencv2/imgproc.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace docscan;

namespace {

CropFilter makeCrop() { return CropFilter(Settings("crop", CropFilter::info().params, {})); }

/// A white page with a few lines of "text", photographed at an angle on a brown table.
cv::Mat photoOfPage()
{
    cv::Mat photo(1200, 900, CV_8UC3, cv::Scalar(60, 110, 160));
    const std::vector<cv::Point> page{{180, 150}, {760, 210}, {700, 1050}, {120, 980}};
    cv::fillConvexPoly(photo, page, cv::Scalar(240, 240, 240));
    for (int line = 0; line < 12; ++line)
        cv::line(photo, {260, 330 + 50 * line}, {600, 350 + 50 * line}, cv::Scalar(30, 30, 30), 4);
    return photo;
}

} // namespace

TEST_CASE("crop cuts the page out of the photo and straightens it")
{
    const cv::Mat page = makeCrop().apply(photoOfPage());

    const double aspect = double(page.rows) / page.cols;
    CHECK(aspect > 1.3);
    CHECK(aspect < 1.6);

    // No table is left: every border of the result is paper.
    const int band = 10;
    for (const cv::Rect border : {cv::Rect(0, 0, page.cols, band), cv::Rect(0, page.rows - band, page.cols, band),
                                  cv::Rect(0, 0, band, page.rows), cv::Rect(page.cols - band, 0, band, page.rows)}) {
        const cv::Scalar mean = cv::mean(page(border));
        CHECK(mean[0] > 220);
        CHECK(mean[2] > 220);
    }
}

TEST_CASE("crop keeps the photo when there is no page")
{
    Log::setLevel(Log::Level::Quiet);
    const cv::Mat blank(600, 400, CV_8UC3, cv::Scalar(90, 90, 90));
    CHECK(makeCrop().apply(blank).size() == blank.size());
    Log::setLevel(Log::Level::Normal);
}
