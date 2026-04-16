#pragma once

/// @file FrameFormatter.h
/// @brief Custom spdlog flag '%@frame' that injects the current frame number,
///        and global frame counter management.

#include <atomic>
#include <cstdint>
#include <memory>

#include <spdlog/pattern_formatter.h>

namespace Diagnostics
{

/// Custom spdlog flag formatter.
/// Usage in pattern string: "[F%@frame]" → "[F00004823]"
class FrameFlag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override;

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override;
};

void IncrementFrameNumber();
uint64_t GetFrameNumber();

} // namespace Diagnostics
