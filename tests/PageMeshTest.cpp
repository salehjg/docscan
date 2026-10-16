#include "vision/PageMesh.hpp"

#include <opencv2/imgproc.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace docscan;
using Catch::Matchers::WithinAbs;

namespace {

/// A rows x columns mesh spread evenly over `area` of the photo.
cv::Mat evenMesh(cv::Rect2f area, int rows = 5, int columns = 4)
{
    cv::Mat points(rows, columns, CV_32FC2);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            points.at<cv::Point2f>(r, c) = {area.x + area.width * float(c) / float(columns - 1),
                                            area.y + area.height * float(r) / float(rows - 1)};
        }
    }
    return points;
}

} // namespace

TEST_CASE("a flat mesh cuts its rectangle out of the photo")
{
    cv::Mat photo(300, 400, CV_8UC3);
    cv::randu(photo, 0, 255);
    cv::GaussianBlur(photo, photo, {5, 5}, 0);
    const PageMesh mesh(evenMesh({100, 50, 200, 160}));

    const cv::Mat flat = mesh.unwarp(photo, {201, 161}, 0);
    cv::Mat difference;
    cv::absdiff(flat, photo(cv::Rect(100, 50, 201, 161)), difference);
    double worst = 0;
    cv::minMaxLoc(difference.reshape(1), nullptr, &worst);
    CHECK(worst <= 1);

    CHECK_THAT(mesh.size().width, WithinAbs(200, 1e-3));
    CHECK_THAT(mesh.size().height, WithinAbs(160, 1e-3));
    const Quad outline = mesh.outline();
    CHECK(outline.corners()[0] == cv::Point2f(100, 50));
    CHECK(outline.corners()[2] == cv::Point2f(300, 210));
}

TEST_CASE("trimming leaves out the edges of the page")
{
    cv::Mat photo(300, 400, CV_8UC1, cv::Scalar(0));
    photo(cv::Rect(110, 60, 181, 141)).setTo(255);
    const PageMesh mesh(evenMesh({100, 50, 200, 160}));
    const cv::Mat flat = mesh.unwarp(photo, {180, 128}, 0.1);
    CHECK(cv::countNonZero(flat < 255) == 0); // the dark frame around the white area is cut away
}

TEST_CASE("only a mesh that can be a page is plausible")
{
    const cv::Size photo(400, 300);
    CHECK(PageMesh(evenMesh({20, 20, 360, 260})).plausible(photo));
    CHECK_FALSE(PageMesh(evenMesh({150, 100, 60, 50})).plausible(photo)); // too small

    cv::Mat folded = evenMesh({20, 20, 360, 260});
    std::swap(folded.at<cv::Point2f>(2, 1), folded.at<cv::Point2f>(2, 2)); // a cell turned inside out
    CHECK_FALSE(PageMesh(folded).plausible(photo));

    cv::Mat mirrored;
    cv::flip(evenMesh({20, 20, 360, 260}), mirrored, 1); // rows running right to left
    CHECK_FALSE(PageMesh(mirrored).plausible(photo));
}

TEST_CASE("a mesh reaching the photo's edge touches its border")
{
    const cv::Size photo(400, 300);
    CHECK_FALSE(PageMesh(evenMesh({40, 40, 320, 220})).touchesBorder(photo));
    CHECK(PageMesh(evenMesh({2, 40, 320, 220})).touchesBorder(photo));
}
