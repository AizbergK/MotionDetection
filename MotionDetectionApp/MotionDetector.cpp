#include "MotionDetector.hpp"

MotionDetector::MotionDetector() : 
    m_initialized{ false },
	m_min_area{ 500.0 }
{
}

bool MotionDetector::detect(const cv::Mat& frame)
{
    cv::Mat gray, blurred;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, cv::Size(21, 21), 0);

    if (!m_initialized)
    {
        m_previous_frame = blurred;
        m_initialized = true;
        return false;
    }

    cv::Mat frame_delta, thresh;
    cv::absdiff(m_previous_frame, blurred, frame_delta);
    cv::threshold(frame_delta, thresh, 25, 255, cv::THRESH_BINARY);
    cv::dilate(thresh, thresh, cv::Mat(), cv::Point(-1, -1), 2);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    m_previous_frame = blurred;
    for (const auto& contour : contours)
    {
        if (cv::contourArea(contour) >= m_min_area)
        {
            return true;
        }
    }
    return false;
}