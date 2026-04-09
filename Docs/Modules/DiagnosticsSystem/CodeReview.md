# DiagnosticsSystem Code Review

Code review findings for the Diagnostics module, organized by severity.

**Review scope**: All files under `Src/Core/Diagnostics/`, `Src/GUI/Panels/ConsolePanel.*`,
`Tests/Contract/Core/TestDiagnostics*.cpp`, and `Tests/Support/DiagnosticsTestSupport.h`.

---

## Summary

The Diagnostics module is well-engineered. Thread safety is handled correctly
throughout, the CAS-based flood prevention macros are textbook-correct, and the
POSIX signal handler is genuinely async-signal-safe. The JSON sink's "always
registered, toggle via file open/close" design elegantly avoids a real data race.
Test coverage is thorough and includes edge cases like async overflow behavior
and compile-time stripping verification.

Most of the findings below are improvement opportunities rather than release-blocking
defects. The main exception is H-3: the fatal crash path currently does not honor the
design's promised synchronous async-log drain before termination.

---

## High Severity

### H-1: ImGuiConsoleSink has redundant double-lock design

**Files**: `ImGuiConsoleSink.h:25`, `ImGuiConsoleSink.cpp:22-48`

`ImGuiConsoleSink` inherits `base_sink<std::mutex>`, so spdlog acquires
`base_sink::mutex_` before calling `sink_it_()`. Inside `sink_it_()`, the code
acquires a second lock (`m_BufferMutex`) to protect `m_Entries`. Meanwhile,
`GetEntries()` and `Clear()` only lock `m_BufferMutex`.

This is safe (no deadlock, consistent lock ordering), but the double-lock means:
- `sink_it_()` holds two mutexes simultaneously, increasing contention.
- `GetEntries()` and `Clear()` can interleave with `sink_it_()` in a way where
  `base_sink::mutex_` is held but `m_BufferMutex` is not yet acquired.

**Options**:
1. Remove `m_BufferMutex` entirely and have `GetEntries()`/`Clear()` lock
   `mutex_` (the base_sink mutex) instead. Simpler, single lock.
2. Keep the double-lock if the intent is to avoid blocking the ImGui thread on
   other sinks' `sink_it_()` calls. In this case, document the lock hierarchy.

### H-2: ConsolePanel::DrawLogEntries uses fixed-size stack buffer for log lines

**File**: `ConsolePanel.cpp:238`

```cpp
char lineBuf[1024];
snprintf(lineBuf, sizeof(lineBuf), "%s [%s] %s %s", ...);
```

If timestamp + category + level tag + message exceeds 1024 bytes, the output is
silently truncated. This is likely for callstack messages or long shader error
strings.

**Recommendation**: Use `fmt::format` or `std::string` + `ImGui::TextUnformatted`.

### H-3: Crash handler does not synchronously drain async logs before termination

**Files**: `CrashHandler.cpp:150-159`, `CrashHandler.cpp:167-202`

The design docs explicitly say the fatal path should use `spdlog::shutdown()` so the
async queue is drained synchronously before the process terminates. The actual crash
path does not do that:

```cpp
Logger::Flush();
TerminateAfterCrash();
```

and:

```cpp
[[noreturn]] void TerminateAfterCrash()
{
    Diagnostics::Logger::Flush();
    TerminateProcess(...); // or _exit(...)
}
```

For an async logger, `logger->flush()` is only a queued request. It does not guarantee
that the worker thread has finished writing all pending messages before the process is
killed. That means the crash-time `critical(...)` logs emitted by `HandleCrash()` may be
lost even though the design document promises preserved diagnostics on fatal exit.

This is more than a documentation mismatch: it weakens the central reason the crash
handler exists in the first place.

**Recommendation**: introduce a dedicated fatal-shutdown path that synchronously drains
the async logging backend before termination, or narrow the design docs if that cannot be
made safe on all crash paths. `Logger::Flush()` alone is not sufficient.

---

## Medium Severity

### M-1: JsonLineSink::sink_it_ allocates ostringstream per message

**File**: `JsonLineSink.cpp:170`

Each call constructs a `std::ostringstream`, which typically involves heap
allocation. This runs on the async worker thread so it does not block the render
thread, but it limits log throughput.

**Recommendation**: Use `fmt::memory_buffer` or write directly to `m_File` via
a reusable buffer.

### M-2: GetEntries() copies all 1024 entries every frame

**File**: `ImGuiConsoleSink.cpp:10-14`, `ConsolePanel.cpp:222`

`GetEntries()` returns a `std::vector<ConsoleLogEntry>` by value. Each
`ConsoleLogEntry` has 4 `std::string` members. At 1024 entries, this is ~4096
string copies per frame (at 60fps: ~245K string copies/sec).

**Recommendation**: Use a dirty flag or generation counter. Only re-copy when
entries have changed. Alternatively, provide a `ForEachEntry(callback)` method
that iterates under the lock without copying.

### M-3: Level filter semantics are inconsistent in ConsolePanel

**File**: `ConsolePanel.cpp:56-77`

```cpp
case 1: return level == spdlog::level::trace;    // exact match
case 2: return level == spdlog::level::debug;    // exact match
case 3: return level >= spdlog::level::info;     // threshold
case 4: return level >= spdlog::level::warn;     // threshold
case 5: return level >= spdlog::level::err;      // threshold
case 6: return level >= spdlog::level::critical; // threshold
```

Trace and Debug use exact matching while Info and above use threshold matching.
This means selecting "Trace" shows only trace messages, but selecting "Info"
shows info + warn + error + critical.

**Recommendation**: Either use threshold semantics for all levels (which is the
more common UX pattern) or document the mixed behavior in the UI.

### M-4: ReadLogTail reads the entire file to extract 80 lines

**File**: `CrashHandler.cpp:50-72`

`ReadLogTail` reads every line of the log file into a deque, keeping only the
last `maxLines`. For a 1MB log file, this reads the full file sequentially.

**Impact**: Low in practice -- this only runs once during a crash. But for
completeness, a reverse-read or seek-from-end approach would be more efficient.

### M-5: Logger bootstrap still relies on exception-throwing filesystem / spdlog APIs

**File**: `Logger.cpp:70-112`

The diagnostics design explicitly avoids C++ exceptions in engine code, but
`Logger::Init()` still uses several operations that can throw:

- `std::filesystem::create_directories(g_LogFilePath.parent_path())`
- `spdlog::sinks::rotating_file_sink_mt(...)`
- `spdlog::init_thread_pool(...)`
- async logger construction / registration

There is no local error handling around that block. If log-path creation or sink setup
fails during startup, initialization can unwind with an exception before the diagnostics
system is even available to report the problem.

That makes diagnostics bootstrap one of the places where the module currently fails to
live up to its own "no exceptions on engine hot paths / all failures produce diagnostics"
story.

**Recommendation**: use `std::error_code` for directory creation and wrap spdlog setup in
`try/catch` with a minimal `stderr` fallback. At minimum, keep `g_Initialized` false on
all partial-init failures and avoid silently leaving the logger in a half-constructed
state.

---

## Low Severity

### L-1: GetConsoleSink() is not protected by g_LoggerMutex

**File**: `Logger.cpp:203-206`

```cpp
Ref<ImGuiConsoleSink> Logger::GetConsoleSink()
{
    return s_ConsoleSink;
}
```

All other public accessors (e.g., `GetLogFilePath`, `IsJsonSinkEnabled`,
`GetJsonSinkPath`) lock `g_LoggerMutex`. `GetConsoleSink` does not. In practice
this is safe because `s_ConsoleSink` is only modified during `Init`/`Shutdown`,
but the inconsistency could mislead future maintainers.

### L-2: RTRLAB_VERIFY captures by reference unconditionally

**File**: `Assert.h:65`

```cpp
#define RTRLAB_VERIFY(expr) \
    ([&]() { ... }())
```

The `[&]` capture is unnecessary when `expr` is a pure expression with no
external references. While the compiler will likely optimize away empty captures,
using `[&]` as a blanket pattern could capture more than intended in complex
macro expansions.

### L-3: POSIX crash file name lacks timestamp, overwrites on consecutive crashes

**File**: `PosixCrashHandler.cpp:157`

The signal handler writes to a fixed filename `RTRLab_posix_signal_crash.txt`.
Consecutive crashes overwrite the previous report.

**Context**: This is a deliberate trade-off -- `localtime_r` is not
async-signal-safe, so generating a timestamp inside the signal handler would be
unsafe. The filename is pre-computed at `Init()` time.

**Mitigation**: Pre-compute a timestamp at `Init()` time and include it in the
filename (e.g., `RTRLab_posix_signal_crash_20260325_143201.txt`). This would
distinguish crash files from different sessions, though not from multiple crashes
within the same session.

### L-4: Win32 SymInitialize is never refreshed for dynamically loaded modules

**File**: `Win32Callstack.cpp:42-47`

`SymInitialize(process, nullptr, TRUE)` is called once on first callstack capture.
If DLLs are loaded after this point, their symbols will not be available for
symbolication.

**Impact**: Minimal for the current project (no plugin system).

### L-5: ENSURE macros lack [[unlikely]] on the condition check

**File**: `Assert.h:75-90`

`RTRLAB_ASSERT` uses `if (!(condition)) [[unlikely]]`, but `RTRLAB_ENSURE` uses
`if (!(condition))` without the attribute. Since ENSURE also represents an
unexpected failure condition, `[[unlikely]]` would be appropriate for consistent
branch prediction hinting.

### L-6: Console command reports JSON sink enabled even when enable failed

**File**: `ConsolePanel.cpp:393-395`

```cpp
Diagnostics::Logger::EnableJsonSink(path);
LOG_INFO_CAT(LogCategory::ImGui, "JSON log sink enabled: {}",
             Diagnostics::Logger::GetJsonSinkPath().string());
```

`EnableJsonSink()` can fail and intentionally leaves the sink disabled. The core logger
does emit an error in that case, but the console command still unconditionally prints a
success-style "enabled" message immediately afterward. If the path is invalid, the user
can see contradictory feedback.

This is a UI/reporting bug rather than a core logging bug, but it makes debugging sink
configuration harder than it needs to be.

**Recommendation**: check `IsJsonSinkEnabled()` after the call and emit either a success
message with the resolved path or a warning/error that the enable request failed.

---

## Positive Findings

### P-1: ONCE/THROTTLE CAS implementation is correct

The `compare_exchange_strong` (ONCE) and `compare_exchange_weak` loop (THROTTLE)
correctly eliminate the TOCTOU race that existed in the earlier load+store design.
Relaxed memory ordering is appropriate -- the only consequence of delayed visibility
is at most one extra log message.

### P-2: JSON sink "always registered" design avoids a real data race

The async worker iterates the sink vector without external locking. The earlier
design that added/removed the JSON sink at runtime was a data race. The current
design of toggling via file open/close under `base_sink::mutex_` is the correct
fix.

### P-3: POSIX signal handler is genuinely async-signal-safe

The handler uses only `write()`, `fsync()`, `close()`, `open()`, `backtrace()`,
`backtrace_symbols_fd()`, `kill()`, `_exit()`. String formatting uses hand-rolled
functions on a stack buffer with no heap allocation. The alternate stack ensures
the handler can run even after stack overflow.

### P-4: Compile-time stripping test uses side-effect counter

`TestDiagnosticsMinLevel.cpp` verifies that stripped macros do not evaluate their
arguments by checking an atomic side-effect counter. This is a precise test that
catches real bugs (e.g., if the macro were `if (false) { log(...); }` instead of
`((void)0)`, arguments would still be evaluated).

### P-5: Crash handler re-entrance protection

`g_HandlingCrash` as an `atomic_flag` with `test_and_set` ensures that if the
crash handler itself triggers a crash (e.g., during callstack capture), the
recursive invocation immediately terminates rather than entering an infinite loop.

### P-6: Thorough test coverage

The test suite covers: basic logging, file rotation, JSON sink enable/disable/shutdown,
async overflow behavior, ERR_FAIL_COND return values, ENSURE non-fatality, crash
handler idempotency, crash directory resolution, and compile-time level stripping.
The `WaitForSettledEntries` helper correctly handles the asynchronous nature of the
logging pipeline.
