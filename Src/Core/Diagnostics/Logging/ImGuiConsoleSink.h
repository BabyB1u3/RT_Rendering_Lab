#pragma once

/// @file ImGuiConsoleSink.h
/// @brief Custom spdlog sink that pushes log messages into a ring buffer
///        for display in the ImGui debug console.

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <spdlog/sinks/base_sink.h>

namespace Diagnostics
{

    struct ConsoleLogEntry
    {
        spdlog::level::level_enum Level;
        std::string Category;
        std::string Message;
        std::string Timestamp;
    };

    class ImGuiConsoleSink : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        std::vector<ConsoleLogEntry> GetEntries() const;
        void Clear();

    protected:
        void sink_it_(const spdlog::details::log_msg &msg) override;
        void flush_() override {}

    private:
        static constexpr size_t kMaxEntries = 1024;
        mutable std::mutex m_BufferMutex;
        std::deque<ConsoleLogEntry> m_Entries;
    };

} // namespace Diagnostics
