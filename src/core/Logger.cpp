#include "Logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>

Ref<spdlog::logger> Logger::s_Logger;

void Logger::Init()
{
    s_Logger = spdlog::stdout_color_mt("RTRLab");
    s_Logger->set_level(spdlog::level::trace);
    s_Logger->set_pattern("[%T] [%^%l%$] %v");
}
