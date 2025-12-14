#ifndef _WRITER_
#define _WRITER_

#include <iostream>
#include <queue>
#include <thread>

#include <opencv2/opencv.hpp>

#include "SignalInterface.hpp"

class Writer
{
public:
    Writer(SignalInterface& sig_int, const int width, const int height, const int fps);
    ~Writer();

    void run();
    void stop();

private:
    const int m_width;
    const int m_height;
    const int m_fps;

    bool m_running;
    bool m_status_running;

    FILE* m_ffmpeg;
    std::thread m_writer_thread;
    SignalInterface& m_signal_interface;
    std::unique_ptr<std::queue<cv::Mat>> m_handle;
};

#endif