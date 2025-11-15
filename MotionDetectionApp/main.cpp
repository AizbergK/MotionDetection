#include <queue>
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <iostream>
#include <stdio.h>

#include <opencv2/opencv.hpp>

//raspberry PI build
//g++ -std=c++20 -O3 -march=native -mtune=native -flto -pipe ./MotionDetectionApp/main.cpp -o ./release/motionApp `pkg-config --cflags --libs opencv4`

FILE* open_ffmpeg(const std::string& cmd) 
{
#ifdef _WIN32
    // Windows uses _popen and _pclose
    return _popen(cmd.c_str(), "wb");
#else
    // Linux/Unix uses popen and pclose
    return popen(cmd.c_str(), "w");
#endif
}

FILE* close_ffmpeg(FILE* ffmpeg)
{
#ifdef _WIN32
    _pclose(ffmpeg);
#else
    pclose(ffmpeg);
#endif
    return nullptr;
}

std::string build_ffmpeg_cmd(int m_width, int m_height, int fps, std::string file_name) 
{
#ifdef _WIN32
    // Windows: use libx264 software encoder
    return "ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt bgr24 "
           "-s " + std::to_string(m_width) + "x" + std::to_string(m_height) + " "
           "-r " + std::to_string(fps) + " -i - "
           "-an -vcodec libx264 -preset veryfast -crf 23 "
           "-pix_fmt yuv420p -benchmark D:\\Recordings\\" + file_name + ">nul 2>&1 2>&1";
#else
    // Raspberry Pi 4: use hardware encoder (v4l2m2m)
    return "ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt bgr24 "
           "-s " + std::to_string(m_width) + "x" + std::to_string(m_height) + " "
           "-r " + std::to_string(fps) + " -i - "
           "-an -c:v h264_v4l2m2m -b:v 2M "
           "-pix_fmt yuv420p -benchmark ./" + file_name + " >/dev/null 2>&1";
#endif
}

std::string make_timestamped_filename() {
    // Get current time
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // Convert to local time
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t); // Windows
#else
    localtime_r(&t, &tm); // Linux / Raspberry Pi
#endif

    // Format: YYYY-MM-DD_HH-MM-SS.mp4
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return oss.str();
}

class MotionDetector
{
public:
    MotionDetector() : m_initialized{ false } {}
    bool detect(const cv::Mat& frame)
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
private:
    cv::Mat m_previous_frame;
    bool m_initialized;
    const double m_min_area{ 500.0 };
};

class SignalInterface
{
public:
    void move_to(std::unique_ptr<std::queue<cv::Mat>> to_add)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_writer_queue.emplace(std::move(to_add));
    }

    std::unique_ptr<std::queue<cv::Mat>> get_next()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
		std::unique_ptr<std::queue<cv::Mat>> to_return{ m_writer_queue.front().release() };
        m_writer_queue.pop();
		return to_return;
    }

    bool is_queue_empty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_writer_queue.empty();
    }
private:
    std::queue<std::unique_ptr<std::queue<cv::Mat>>> m_writer_queue;
    std::mutex m_mutex;
};

class Writer
{
public:
	Writer(SignalInterface& sig_int, const int width, const int height) : m_handle{ nullptr }, signal_interface{ sig_int }, m_width{ width }, m_height{ height }{}
    ~Writer(){ m_ffmpeg = close_ffmpeg(m_ffmpeg); }

    void start_writer()
    {

        while (m_running || !signal_interface.is_queue_empty() || m_handle != nullptr)
        {
            if (signal_interface.is_queue_empty() && m_handle == nullptr)
            {
                if (status_running)
                {
                    std::cout << "Waiting for new data to pipe in...\n";
                }
                else
                {
                    std::cout << "Waiting to start recording...\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
            else
            {
                if (m_ffmpeg == nullptr)
                {
                    std::string cmd{ build_ffmpeg_cmd(m_width, m_height, m_fps, make_timestamped_filename()) };
                    m_ffmpeg = open_ffmpeg(cmd.c_str());
                    status_running = true;
                }

                if (m_handle.get() != nullptr)
                {
                    if (m_handle->empty())
                    {
                        m_handle = nullptr;
                    }
                    else
                    {
                        fwrite(m_handle->front().data, 1, m_width * m_height * 3, m_ffmpeg);
                        m_handle->pop();

                    }
                }
                else
                {
                    std::cout << "getting data to write...\n";
                    m_handle = signal_interface.get_next();
					if (m_handle.get()->size() == 0)
                    {
                        std::cout << "Received empty queue, ending current recording segment.\n";
						status_running = false;
                        m_handle = nullptr;
                        m_ffmpeg = close_ffmpeg(m_ffmpeg);
                    }
                }
            }
        }
    }

    void stop()
    {
        m_running = false;
	}

    SignalInterface& signal_interface;
private:
    std::unique_ptr<std::queue<cv::Mat>> m_handle{ nullptr };
	bool m_running{ true };
	bool status_running{ false };
    const int m_width;
    const int m_height;
    const int m_fps{ 30 };
    FILE* m_ffmpeg{ nullptr };
};

int main()
{
	const int fps{ 30 };
	const int seconds_to_buffer{ 3 };

    cv::VideoCapture cap(0);
    if (!cap.isOpened()) 
    {
        std::cerr << "Error: Cannot open camera\n";
        return -1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, fps);
	const int seconds_buffer{ fps * seconds_to_buffer };

    cv::Mat frame;
    if (!cap.read(frame))
    {
        std::cerr << "Error: Cannot read frame from camera\n";
        exit(-1);
    }
    
    SignalInterface signal_interface;
    Writer writer{ signal_interface, frame.size().width, frame.size().height };
    std::thread writer_thread{ &Writer::start_writer, &writer };
    std::unique_ptr<std::queue<cv::Mat>> frame_queue{ std::make_unique<std::queue<cv::Mat>>() };
	MotionDetector motion_detector;

	bool motion_detected{ false };
	bool is_recording{ false };
	std::chrono::high_resolution_clock::time_point last_motion_timestamp{};
    while(true)
    {
        if (!cap.read(frame))
        {
            std::cerr << "Error: Cannot read frame from camera\n";
            exit(-1);
        }
        frame_queue->emplace(frame.clone());

		if (motion_detector.detect(frame))
        {
            last_motion_timestamp = std::chrono::high_resolution_clock::now();
			is_recording = true;
            if (frame_queue->size() >= seconds_buffer)
            {
                signal_interface.move_to(std::move(frame_queue));
                frame_queue = std::make_unique<std::queue<cv::Mat>>();
            }
        }
        else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_motion_timestamp).count() <= 1000)
        {
            continue;
        }
        else
        {
            if (is_recording)
            {
                is_recording = false;
                if (frame_queue->size() != 0)
                {
                    signal_interface.move_to(std::move(frame_queue));
                    frame_queue = std::make_unique<std::queue<cv::Mat>>();
                }
				signal_interface.move_to(std::move(frame_queue)); // pass an empty queue to indicate end of recording
                frame_queue = std::make_unique<std::queue<cv::Mat>>();
            }
            if (frame_queue->size() > seconds_buffer)
            {
                frame_queue->pop();
            }
        }
    }
    cap.release();
    signal_interface.move_to(std::move(frame_queue));

    writer.stop();
    writer_thread.join();

    return 0;
}