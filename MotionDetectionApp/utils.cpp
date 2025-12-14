#include "utils.hpp"
#include <iomanip>

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