#include "App.hpp"

App::App(const int width, const int height, const int fps, const int buffer_as_seconds)	: 
	m_width					{ width },
	m_height				{ height },
	m_fps					{ fps },
	m_buffer_as_seconds		{ buffer_as_seconds },
	m_buffer_as_frames		{ fps * buffer_as_seconds },
	m_signal_interface		{  },
	m_motion_detector		{  },
	m_writer				{ m_signal_interface, width, height, fps },
	m_capture_device        { 0 },
	m_frame                 {  },
	m_frame_queue			{ std::make_unique<std::queue<cv::Mat>>() }
{
	// Initialize camera
    if (!m_capture_device.isOpened())
    {
		throw std::runtime_error("Cannot open camera");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    m_capture_device.set(cv::CAP_PROP_FRAME_WIDTH, m_width);
    m_capture_device.set(cv::CAP_PROP_FRAME_HEIGHT, m_height);
    m_capture_device.set(cv::CAP_PROP_FPS, m_fps);

    if (!m_capture_device.read(m_frame))
    {
		throw std::runtime_error("Cannot read frame from camera");
    }
}

App::~App()
{
	// Cleanup
	m_capture_device.release();
	m_signal_interface.move_to(std::move(m_frame_queue));
	m_writer.stop();
}

void App::run()
{
	std::chrono::high_resolution_clock::time_point last_motion_timestamp{};
	
	bool is_recording{ false };
	while (true)
	{
		if (!m_capture_device.read(m_frame))
		{
			throw std::runtime_error("Cannot read frame from camera");
		}
		m_frame_queue->emplace(m_frame.clone());

		if (m_motion_detector.detect(m_frame))
		{
			last_motion_timestamp = std::chrono::high_resolution_clock::now();
			is_recording = true;
			if (m_frame_queue->size() >= m_buffer_as_frames)
			{
				m_signal_interface.move_to(std::move(m_frame_queue));
				m_frame_queue = std::make_unique<std::queue<cv::Mat>>();
			}
		}
		else
		{
			auto elapsed_since_last{ std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_motion_timestamp).count() };
			if (elapsed_since_last <= 3000)
			{
				continue;
			}
			if (is_recording)
			{
				is_recording = false;
				if (m_frame_queue->size() != 0)
				{
					m_signal_interface.move_to(std::move(m_frame_queue));
					m_frame_queue = std::make_unique<std::queue<cv::Mat>>();
				}
				m_signal_interface.move_to(std::move(m_frame_queue)); // pass an empty queue to indicate end of recording
				m_frame_queue = std::make_unique<std::queue<cv::Mat>>();
			}
			if (m_frame_queue->size() > m_buffer_as_frames)
			{
				m_frame_queue->pop();
			}
		}
	}
}