#pragma once

/// @file Callstack.h
/// @brief Lightweight callstack capture for diagnostics output.

#include <string>

namespace Diagnostics
{

std::string CaptureCallstack(int framesToSkip = 0, int maxFrames = 32);

} // namespace Diagnostics
