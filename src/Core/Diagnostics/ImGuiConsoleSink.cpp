#include "Core/Diagnostics/ImGuiConsoleSink.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace Diagnostics
{

    std::vector<ConsoleLogEntry> ImGuiConsoleSink::GetEntries() const
    {
        std::lock_guard<std::mutex> lock(m_BufferMutex);
        return {m_Entries.begin(), m_Entries.end()};
    }

    void ImGuiConsoleSink::Clear()
    {
        std::lock_guard<std::mutex> lock(m_BufferMutex);
        m_Entries.clear();
    }

    void ImGuiConsoleSink::sink_it_(const spdlog::details::log_msg &msg)
    {
        // Extract timestamp without fmt chrono (avoids compile-time format issues).
        auto timeT = std::chrono::system_clock::to_time_t(msg.time);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      msg.time.time_since_epoch()) %
                  1000;
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &timeT);
#else
        localtime_r(&timeT, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%T") << '.' << std::setfill('0') << std::setw(3) << ms.count();

        ConsoleLogEntry entry;
        entry.Level = msg.level;
        entry.Category = std::string(msg.logger_name.data(), msg.logger_name.size());
        entry.Message = std::string(msg.payload.data(), msg.payload.size());
        entry.Timestamp = oss.str();

        std::lock_guard<std::mutex> lock(m_BufferMutex);
        m_Entries.push_back(std::move(entry));
        while (m_Entries.size() > kMaxEntries)
            m_Entries.pop_front();
    }

} // namespace Diagnostics
