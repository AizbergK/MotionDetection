#ifndef _SIGNAL_INTERFACE_
#define _SIGNAL_INTERFACE_

#include <opencv2/opencv.hpp>

#include <memory>
#include <queue>


class SignalInterface
{
public:
	SignalInterface() = default;
	~SignalInterface() = default;

    void move_to(std::unique_ptr<std::queue<cv::Mat>> to_add);
    std::unique_ptr<std::queue<cv::Mat>> get_next();
    bool is_queue_empty();

private:
    std::queue<std::unique_ptr<std::queue<cv::Mat>>> m_writer_queue;
    std::mutex m_mutex;
};

#endif