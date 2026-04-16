#include "Core/Diagnostics/Logging/FrameFormatter.h"

#include <spdlog/spdlog.h>

namespace Diagnostics
{

namespace
{
std::atomic<uint64_t> g_FrameNumber{0};
}

void FrameFlag::format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest)
{
    auto str = fmt::format("{:08}", g_FrameNumber.load(std::memory_order_relaxed));
    dest.append(str.data(), str.data() + str.size());
}

std::unique_ptr<spdlog::custom_flag_formatter> FrameFlag::clone() const
{
    return std::make_unique<FrameFlag>();
}

void IncrementFrameNumber()
{
    g_FrameNumber.fetch_add(1, std::memory_order_relaxed);
}

uint64_t GetFrameNumber()
{
    return g_FrameNumber.load(std::memory_order_relaxed);
}

} // namespace Diagnostics
