#pragma once

/// @file Debugger.h
/// @brief Cross-platform debugger detection and trap helpers for diagnostics.

namespace Diagnostics
{

bool IsDebuggerAttached();
void BreakInDebuggerIfAttached();

} // namespace Diagnostics
