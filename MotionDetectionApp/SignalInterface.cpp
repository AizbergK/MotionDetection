#include "SignalInterface.hpp"

void SignalInterface::move_to(std::unique_ptr<std::queue<cv::Mat>> to_add)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_writer_queue.emplace(std::move(to_add));
}

std::unique_ptr<std::queue<cv::Mat>> SignalInterface::get_next()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::unique_ptr<std::queue<cv::Mat>> to_return{ m_writer_queue.front().release() };
	m_writer_queue.pop();
	return to_return;
}

bool SignalInterface::is_queue_empty()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_writer_queue.empty();
}