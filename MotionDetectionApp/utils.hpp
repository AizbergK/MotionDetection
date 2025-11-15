#ifndef _UTILS_
#define _UTILS_

#include <string>
#include <chrono>

FILE* open_ffmpeg(const std::string& cmd);
FILE* close_ffmpeg(FILE* ffmpeg);
std::string build_ffmpeg_cmd(int m_width, int m_height, int fps, std::string file_name);
std::string make_timestamped_filename();

#endif