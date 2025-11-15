#ifndef _MOTION_DETECTOR_
#define _MOTION_DETECTOR_

#include <vector>
#include <opencv2/opencv.hpp>

class MotionDetector
{
public:
    MotionDetector();
    bool detect(const cv::Mat& frame);

private:
    cv::Mat m_previous_frame;
    bool m_initialized;
    const double m_min_area;
};

#endif