#pragma once

/// @file JsonLineSink.h
/// @brief Structured JSON Lines (.jsonl) spdlog sink for machine-readable log output.
///
/// Created in disabled state at Logger::Init(). Enable/Disable open/close the
/// output file under the base_sink mutex, so there is no race with the async
/// logger backend that calls sink_it_() through the same mutex.

#include <filesystem>
#include <fstream>
#include <mutex>

#include <spdlog/sinks/base_sink.h>

namespace Diagnostics
{

    /// Writes one JSON object per log message, one per line (JSON Lines format).
    /// Output is parseable by `jq` and other JSON tooling.
    ///
    /// Example output:
    /// {"ts":"2026-03-25T14:32:01.234","frame":4823,"tid":12340,"cat":"Shader","lvl":"error","msg":"Compilation failed"}
    class JsonLineSink : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        JsonLineSink() = default;

        /// Opens the file for writing. Thread-safe (locks the base_sink mutex).
        void Enable(const std::filesystem::path &filePath);

        /// Closes the file. Thread-safe (locks the base_sink mutex).
        void Disable();

        bool IsEnabled() const;

    protected:
        void sink_it_(const spdlog::details::log_msg &msg) override;
        void flush_() override;

    private:
        std::ofstream m_File;
    };

} // namespace Diagnostics
