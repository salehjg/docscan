#include "core/Error.hpp"
#include "core/Log.hpp"
#include "filters/CropFilter.hpp"
#include "vision/UVDocPageFinder.hpp"

#include <opencv2/imgproc.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace docscan;

namespace {

CropFilter makeCrop(const Arguments& arguments = {{"method", "contour"}})
{
    return CropFilter(Settings("crop", CropFilter::info().params, arguments));
}

/// The UVDoc weights named by $DOCSCAN_UVDOC_MODEL, or installed where docscan looks for them.
std::optional<std::filesystem::path> uvdocModel()
{
    if (const char* path = std::getenv("DOCSCAN_UVDOC_MODEL"); path && *path)
        return std::filesystem::path(path);
    return UVDocPageFinder::locate("");
}

/// Every border of `page` is paper: no table is left.
void checkOnlyPaperAtEdges(const cv::Mat& page)
{
    const int band = 10;
    for (const cv::Rect border : {cv::Rect(0, 0, page.cols, band), cv::Rect(0, page.rows - band, page.cols, band),
                                  cv::Rect(0, 0, band, page.rows), cv::Rect(page.cols - band, 0, band, page.rows)}) {
        const cv::Scalar mean = cv::mean(page(border));
        CHECK(mean[0] > 220);
        CHECK(mean[2] > 220);
    }
}

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

/// A page with lines of "text" that fills the whole photo, shaded towards one corner as if by the phone.
cv::Mat photoFilledByPage()
{
    cv::Mat photo(1200, 900, CV_8UC3);
    for (int y = 0; y < photo.rows; ++y) {
        for (int x = 0; x < photo.cols; ++x) {
            const double shade = std::clamp(2.0 * y / photo.rows - double(x) / photo.cols, 0.0, 1.0);
            photo.at<cv::Vec3b>(y, x) = cv::Vec3b::all(cv::saturate_cast<uchar>(235 - 120 * shade));
        }
    }
    for (int line = 0; line < 16; ++line)
        cv::line(photo, {100, 130 + 60 * line}, {700, 130 + 60 * line}, cv::Scalar(30, 30, 30), 3);
    return photo;
}

} // namespace

TEST_CASE("crop cuts the page out of the photo and straightens it")
{
    const cv::Mat page = makeCrop().apply(photoOfPage());

    const double aspect = double(page.rows) / page.cols;
    CHECK(aspect > 1.3);
    CHECK(aspect < 1.6);
    checkOnlyPaperAtEdges(page);
}

TEST_CASE("the UVDoc network cuts the page out of the photo and flattens it")
{
    const auto model = uvdocModel();
    if (!model)
        SKIP("UVDoc model not installed; set DOCSCAN_UVDOC_MODEL to its best_model.pkl");

    const cv::Mat page = makeCrop({{"method", "uvdoc"}, {"model", model->string()}}).apply(photoOfPage());
    const double aspect = double(page.rows) / page.cols;
    CHECK(aspect > 1.3);
    CHECK(aspect < 1.6);
    checkOnlyPaperAtEdges(page);
}

TEST_CASE("asking for the UVDoc network without its model is an error")
{
    CHECK_THROWS_AS(makeCrop({{"method", "uvdoc"}, {"model", "/nonexistent/uvdoc.pkl"}}), UsageError);
}

TEST_CASE("crop keeps a photo the page fills instead of cutting across the paper")
{
    Log::setLevel(Log::Level::Quiet);
    const cv::Mat photo = photoFilledByPage();
    const cv::Mat page = makeCrop().apply(photo);
    Log::setLevel(Log::Level::Normal);

    const double aspect = double(page.rows) / page.cols;
    const double photoAspect = double(photo.rows) / photo.cols;
    CHECK(std::abs(aspect / photoAspect - 1) < 0.05);
    CHECK(page.total() > 0.9 * photo.total());
}

TEST_CASE("crop keeps the photo when there is no page")
{
    Log::setLevel(Log::Level::Quiet);
    const cv::Mat blank(600, 400, CV_8UC3, cv::Scalar(90, 90, 90));
    CHECK(makeCrop().apply(blank).size() == blank.size());
    Log::setLevel(Log::Level::Normal);
}
