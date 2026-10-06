#include "filters/RotateFilter.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>

namespace docscan {

FilterInfo RotateFilter::info()
{
    return {"rotate",
            "turn the page clockwise",
            {ParamSpec::number("angle", 90, -360, 360, "degrees clockwise; 90, 180 and 270 lose no quality")}};
}

RotateFilter::RotateFilter(const Settings& settings)
{
    angle_ = std::fmod(settings.get<double>("angle"), 360.0);
    if (angle_ < 0)
        angle_ += 360.0;
}

cv::Mat RotateFilter::apply(const cv::Mat& image) const
{
    cv::Mat rotated;
    if (angle_ == 0)
        return image;
    if (angle_ == 90)
        cv::rotate(image, rotated, cv::ROTATE_90_CLOCKWISE);
    else if (angle_ == 180)
        cv::rotate(image, rotated, cv::ROTATE_180);
    else if (angle_ == 270)
        cv::rotate(image, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
    else {
        // OpenCV turns counter-clockwise for positive angles.
        const cv::Point2f center(image.cols / 2.0f, image.rows / 2.0f);
        cv::Mat transform = cv::getRotationMatrix2D(center, -angle_, 1.0);
        const cv::Rect2f bounds = cv::RotatedRect({}, image.size(), float(-angle_)).boundingRect2f();
        transform.at<double>(0, 2) += bounds.width / 2.0 - center.x;
        transform.at<double>(1, 2) += bounds.height / 2.0 - center.y;
        cv::warpAffine(image, rotated, transform, bounds.size(), cv::INTER_CUBIC, cv::BORDER_CONSTANT,
                       cv::Scalar::all(255));
    }
    return rotated;
}

} // namespace docscan
