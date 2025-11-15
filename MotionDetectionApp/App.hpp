#ifndef _APP_
#define _APP_

#include <opencv2/opencv.hpp>

#include "SignalInterface.hpp"
#include "MotionDetector.hpp"
#include "Writer.hpp"
#include "utils.hpp"

class App
{
public:
	App(const int width, const int height, const int fps, const int buffer_as_seconds);
	~App();

	void run();

private:
	const int m_width;
	const int m_height;
	const int m_fps;
	const int m_buffer_as_seconds;
	const int m_buffer_as_frames;
	SignalInterface m_signal_interface;
	MotionDetector m_motion_detector;
	Writer m_writer;

	cv::VideoCapture m_capture_device;
	cv::Mat m_frame;
	std::unique_ptr<std::queue<cv::Mat>> m_frame_queue;
};

#endif