#include "Core/Diagnostics/Logging/JsonLineSink.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "Core/Diagnostics/Logging/FrameFormatter.h"

namespace Diagnostics
{

namespace
{
bool OpenJsonFile(std::ofstream& file, const std::filesystem::path& filePath)
{
    if (filePath.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);
        if (ec)
            return false;
    }

    file.open(filePath, std::ios::out | std::ios::app);
    return file.is_open();
}
} // namespace

bool JsonLineSink::Enable(const std::filesystem::path& filePath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (m_File.is_open())
        return true;

    if (!OpenJsonFile(m_File, filePath))
        return false;

    m_DisableWhenFlushed = false;
    m_PendingReopenPath.reset();
    m_PendingControlGeneration = 0;
    return true;
}

uint64_t JsonLineSink::RequestDisable()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (m_File.is_open())
    {
        m_DisableWhenFlushed = true;
        m_PendingReopenPath.reset();
        m_PendingControlGeneration = m_NextControlGeneration++;
        return m_PendingControlGeneration;
    }

    return 0;
}

uint64_t JsonLineSink::RequestReopen(const std::filesystem::path& filePath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!m_File.is_open())
    {
        if (!OpenJsonFile(m_File, filePath))
            return 0;

        m_DisableWhenFlushed = false;
        m_PendingReopenPath.reset();
        m_PendingControlGeneration = 0;
        return 0;
    }

    m_DisableWhenFlushed = true;
    m_PendingReopenPath = filePath;
    m_PendingControlGeneration = m_NextControlGeneration++;
    return m_PendingControlGeneration;
}

void JsonLineSink::Disable()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (m_File.is_open())
    {
        m_File.flush();
        m_File.close();
    }
    m_DisableWhenFlushed = false;
    m_PendingReopenPath.reset();
    if (m_PendingControlGeneration != 0)
    {
        m_CompletedControlGeneration = m_PendingControlGeneration;
        m_PendingControlGeneration = 0;
        m_ControlCv.notify_all();
    }
}

bool JsonLineSink::IsEnabled()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return m_File.is_open();
}

bool JsonLineSink::WaitForControl(uint64_t generation, std::chrono::milliseconds timeout)
{
    if (generation == 0)
        return true;

    std::unique_lock<std::mutex> lock(mutex_);
    return m_ControlCv.wait_for(
        lock, timeout, [this, generation]() { return m_CompletedControlGeneration >= generation; });
}

/// Escapes a string for safe embedding in a JSON value.
static void WriteJsonString(std::ostream& os, std::string_view sv)
{
    os << '"';
    for (char c : sv)
    {
        switch (c)
        {
            case '"':
                os << "\\\"";
                break;
            case '\\':
                os << "\\\\";
                break;
            case '\n':
                os << "\\n";
                break;
            case '\r':
                os << "\\r";
                break;
            case '\t':
                os << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    os << buf;
                }
                else
                {
                    os << c;
                }
                break;
        }
    }
    os << '"';
}

void JsonLineSink::sink_it_(const spdlog::details::log_msg& msg)
{
    // base_sink already holds mutex_ here - safe to check m_File.
    if (!m_File.is_open())
        return;

    auto timeT = std::chrono::system_clock::to_time_t(msg.time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()) % 1000;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &timeT);
#else
    localtime_r(&timeT, &tm);
#endif

    std::ostringstream line;
    line << "{\"ts\":\"";
    line << std::put_time(&tm, "%Y-%m-%dT%T") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    line << '"';

    line << ",\"frame\":" << GetFrameNumber();
    line << ",\"tid\":" << msg.thread_id;

    line << ",\"cat\":";
    WriteJsonString(line, {msg.logger_name.data(), msg.logger_name.size()});

    line << ",\"lvl\":\"" << spdlog::level::to_string_view(msg.level).data() << '"';

    line << ",\"msg\":";
    WriteJsonString(line, {msg.payload.data(), msg.payload.size()});

    line << '}';

    m_File << line.str() << '\n';
}

void JsonLineSink::flush_()
{
    if (m_File.is_open())
    {
        m_File.flush();
        if (m_DisableWhenFlushed)
        {
            m_File.close();
            m_DisableWhenFlushed = false;

            if (m_PendingReopenPath.has_value())
            {
                OpenJsonFile(m_File, *m_PendingReopenPath);
                m_PendingReopenPath.reset();
            }
        }
    }

    if (m_PendingControlGeneration != 0)
    {
        m_CompletedControlGeneration = m_PendingControlGeneration;
        m_PendingControlGeneration = 0;
        m_ControlCv.notify_all();
    }
}

} // namespace Diagnostics
