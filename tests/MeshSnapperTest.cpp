#include "vision/MeshSnapper.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace docscan;
using Catch::Matchers::WithinAbs;

namespace {

const cv::Scalar kTable(60, 110, 160);
const cv::Scalar kPaper(235, 235, 235);
const cv::Scalar kInk(40, 40, 40);

/// A mesh spread evenly over `area`, as a network would give it.
PageMesh meshOver(cv::Rect2f area, int rows = 9, int columns = 7)
{
    cv::Mat points(rows, columns, CV_32FC2);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            points.at<cv::Point2f>(r, c) = {area.x + area.width * float(c) / float(columns - 1),
                                            area.y + area.height * float(r) / float(rows - 1)};
        }
    }
    return PageMesh(points);
}

/// Lines of "handwriting" across `area`.
void scribble(cv::Mat& photo, cv::Rect area)
{
    for (int y = area.y + 10; y < area.y + area.height - 10; y += 30)
        cv::line(photo, {area.x + 5, y}, {area.x + area.width - 5, y + 6}, kInk, 3);
}

cv::Rect2f bounds(const PageMesh& mesh)
{
    std::vector<cv::Point2f> points(mesh.points().begin<cv::Point2f>(), mesh.points().end<cv::Point2f>());
    return cv::boundingRect(points);
}

} // namespace

TEST_CASE("snapping trims a sliver of table left along the page")
{
    cv::Mat photo(1000, 800, CV_8UC3, kTable);
    photo(cv::Rect(100, 100, 600, 800)).setTo(kPaper);
    // The rough mesh reaches 15 pixels into the table at the bottom and right.
    const cv::Rect2f snapped = bounds(MeshSnapper().snap(meshOver({100, 100, 615, 815}), photo));
    CHECK_THAT(snapped.x + snapped.width, WithinAbs(700, 4));
    CHECK_THAT(snapped.y + snapped.height, WithinAbs(900, 4));
    CHECK_THAT(snapped.x, WithinAbs(100, 4));
    CHECK_THAT(snapped.y, WithinAbs(100, 4));
}

TEST_CASE("snapping grows the outline to the photo's edge over a page with writing on it")
{
    cv::Mat photo(1000, 800, CV_8UC3, kPaper); // the page fills the photo
    scribble(photo, {0, 100, 70, 800});        // notes in the margin the rough mesh left out
    const cv::Rect2f snapped = bounds(MeshSnapper().snap(meshOver({80, 30, 690, 940}), photo));
    CHECK(snapped.x < 5);
}

TEST_CASE("snapping does not grow the outline over blank paper, which may be another sheet")
{
    cv::Mat photo(1000, 800, CV_8UC3, kPaper);
    const cv::Rect2f snapped = bounds(MeshSnapper().snap(meshOver({80, 30, 690, 940}), photo));
    CHECK_THAT(snapped.x, WithinAbs(80, 2));
}

TEST_CASE("a soft shadow on the page is not taken for its edge")
{
    cv::Mat photo(1000, 800, CV_8UC3, kTable);
    photo(cv::Rect(100, 100, 600, 800)).setTo(kPaper);
    for (int y = 700; y < 900; ++y) // darkening slowly towards the bottom of the page
        photo(cv::Rect(100, y, 600, 1)).setTo(kPaper * (1 - 0.4 * (y - 700) / 200.0));
    const cv::Rect2f snapped = bounds(MeshSnapper().snap(meshOver({100, 100, 600, 800}), photo));
    CHECK_THAT(snapped.y + snapped.height, WithinAbs(900, 4));
}
