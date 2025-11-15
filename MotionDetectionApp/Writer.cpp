#include <thread>
#include "Writer.hpp"
#include "utils.hpp"


Writer::Writer(SignalInterface& sig_int, const int width, const int height, const int fps)	: 
	m_handle			{ nullptr }, 
	m_signal_interface	{ sig_int }, 
	m_width				{ width }, 
	m_height			{ height },
	m_fps               { fps },
	m_ffmpeg            { nullptr },
	m_running           { true },
	m_status_running    { false },
    m_writer_thread     { std::thread(&Writer::run, this) }
{
}

Writer::~Writer()
{
	m_ffmpeg = close_ffmpeg(m_ffmpeg);
}

void Writer::run()
{

    while (m_running || !m_signal_interface.is_queue_empty() || m_handle != nullptr)
    {
        if (m_signal_interface.is_queue_empty() && m_handle == nullptr)
        {
            if (m_status_running)
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
                m_status_running = true;
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
                m_handle = m_signal_interface.get_next();
                if (m_handle.get()->size() == 0)
                {
                    std::cout << "Received empty queue, ending current recording segment.\n";
                    m_status_running = false;
                    m_handle = nullptr;
                    m_ffmpeg = close_ffmpeg(m_ffmpeg);
                }
            }
        }
    }
}

void Writer::stop()
{
    m_running = false;
}