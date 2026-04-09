# DiagnosticsSystem Internals

A detailed technical reference for the Diagnostics module's internal design,
data flow, threading model, and platform-specific implementation choices.

---

## Table of Contents

- [1. Module Boundaries and Dependency Role](#1-module-boundaries-and-dependency-role)
- [2. Subsystem Overview](#2-subsystem-overview)
- [3. Logging Subsystem Internals](#3-logging-subsystem-internals)
  - [3.1 Initialization and Shutdown Sequence](#31-initialization-and-shutdown-sequence)
  - [3.2 Async Pipeline and Thread Model](#32-async-pipeline-and-thread-model)
  - [3.3 Category System and Lazy Logger Creation](#33-category-system-and-lazy-logger-creation)
  - [3.4 Log Macros and Compile-Time Level Stripping](#34-log-macros-and-compile-time-level-stripping)
  - [3.5 Flood Prevention Internals (ONCE / THROTTLE / COND)](#35-flood-prevention-internals-once--throttle--cond)
  - [3.6 Sink Architecture](#36-sink-architecture)
  - [3.7 Frame Number Injection](#37-frame-number-injection)
  - [3.8 JSON Lines Sink and Control Protocol](#38-json-lines-sink-and-control-protocol)
  - [3.9 ImGui Console Sink](#39-imgui-console-sink)
- [4. Assertion Subsystem Internals](#4-assertion-subsystem-internals)
  - [4.1 Three-Tier Assertion Model](#41-three-tier-assertion-model)
  - [4.2 Assertion Failure Flow](#42-assertion-failure-flow)
  - [4.3 Error Macros (ERR_FAIL_COND)](#43-error-macros-err_fail_cond)
- [5. Crash Handler Internals](#5-crash-handler-internals)
  - [5.1 Platform Registration](#51-platform-registration)
  - [5.2 Crash Sequence and Artifact Generation](#52-crash-sequence-and-artifact-generation)
  - [5.3 POSIX Signal Safety](#53-posix-signal-safety)
  - [5.4 Callstack Capture](#54-callstack-capture)
  - [5.5 Debugger Integration](#55-debugger-integration)
- [6. Debug Console (ConsolePanel)](#6-debug-console-consolepanel)
- [7. Threading and Synchronization Map](#7-threading-and-synchronization-map)
- [8. Comparison with Industry Engines](#8-comparison-with-industry-engines)
  - [8.1 vs Unreal Engine 5](#81-vs-unreal-engine-5)
  - [8.2 vs Godot Engine](#82-vs-godot-engine)
  - [8.3 vs CryEngine / O3DE](#83-vs-cryengine--o3de)

---

## 1. Module Boundaries and Dependency Role

The Diagnostics module sits at the lowest layer of the engine's Core infrastructure.
It has no dependencies on rendering, input, or GUI subsystems -- only on the
Resource/FileSystem module (for resolving log file paths via virtual paths like
`/Saved/logs/`).

Every other engine module depends on Diagnostics. This makes initialization order
critical: `FileSystem::Init()` must precede `Logger::Init()`, which must precede
`CrashHandler::Init()`. Shutdown runs in reverse order.

The module is structured into three subsystems:

```
Core/Diagnostics/
    Logging/     - Async category-based logging with four sinks
    Assert/      - Always-on assertions + error macros
    Crash/       - Platform crash handlers + callstack capture + debugger integration
```

Plus one GUI component that lives outside the Diagnostics directory:

```
GUI/Panels/
    ConsolePanel.h/.cpp  - ImGui debug console (reads from ImGuiConsoleSink)
```

---

## 2. Subsystem Overview

| Subsystem | Key Classes/Files | Responsibility |
|-----------|-------------------|----------------|
| Logging | `Logger`, `LogMacros.h`, `LogCategories.h`, `FrameFormatter`, `JsonLineSink`, `ImGuiConsoleSink` | Async, category-aware, multi-sink logging with compile-time stripping |
| Assert | `Assert.h`, `ErrorMacros.h` | Always-on assertions (ASSERT/VERIFY/ENSURE) + early-return error macros |
| Crash | `CrashHandler`, `Callstack.h`, `Debugger.h`, platform backends | SEH/signal handlers, minidump generation, crash artifact writing |
| Console | `ConsolePanel` | ImGui-based live log viewer with runtime commands |

---

## 3. Logging Subsystem Internals

### 3.1 Initialization and Shutdown Sequence

**Init** (`Logger::Init`):

1. Acquires `g_LoggerMutex` and checks `g_Initialized` to prevent double-init.
2. Resolves the log file path: if empty, uses `FileSystem::ResolveWritePath("/Saved/logs/RTRLab.log")`.
   Falls back to `logs/RTRLab.log` if virtual path resolution fails.
3. Creates four sinks: stdout color, rotating file (1MB / 3 files), ImGuiConsoleSink, JsonLineSink (disabled).
4. Configures the file sink with a custom `spdlog::pattern_formatter` that registers `FrameFlag` as `%@`.
5. Calls `spdlog::init_thread_pool(8192, 1)` -- 8192-slot MPSC queue, 1 worker thread.
6. Creates and registers the "Core" async logger as the first category.
7. Starts periodic flush every 3 seconds via `spdlog::flush_every`.

**Shutdown** (`Logger::Shutdown`):

1. Acquires `g_LoggerMutex`, checks `g_Initialized`.
2. Calls `spdlog::shutdown()` -- this synchronously drains the async queue, flushes all sinks,
   stops the worker thread, stops the periodic flusher, and drops all loggers from the registry.
3. Disables the JSON sink (closes its file).
4. Resets all shared pointers and state flags.

The ordering is critical: `spdlog::shutdown()` must complete before any sink
pointer is released, because the async worker may still be in-flight.

### 3.2 Async Pipeline and Thread Model

```
Caller thread              Async worker thread              Sink outputs
     |                          |                               |
     | logger->info(...)        |                               |
     |--[lock-free enqueue]--->|                               |
     | (~10ns, returns)        |                               |
     |                         | dequeue msg                   |
     |                         | format (pattern_formatter)    |
     |                         |---[lock base_sink mutex]----->|
     |                         |   sink_it_(console)           |
     |                         |   sink_it_(file)              |
     |                         |   sink_it_(imgui ring)        |
     |                         |   sink_it_(json, if enabled)  |
     |                         |---[unlock]                    |
```

Key parameters:
- Queue: 8192 slots (~64KB). Overflows drop oldest messages (`overrun_oldest`).
- Worker threads: 1. File I/O is sequential; more workers add lock contention.
- Within a single category, message ordering is preserved.
- Across categories, slight reordering is possible (both go into the same queue,
  but enqueue order depends on which thread acquires the queue slot first).

### 3.3 Category System and Lazy Logger Creation

Categories are `constexpr const char*` strings defined in `LogCategories.h`.
This design allows subsystems to define additional categories in their own headers
without modifying a central enum.

`Logger::GetLogger(category)`:
1. Acquires `g_LoggerMutex`.
2. Checks `spdlog::get(category)` -- O(1) lookup in spdlog's name registry.
3. On miss, creates a new `spdlog::async_logger` sharing the same 4-sink vector
   and thread pool. Registers it in spdlog's registry.
4. Sets the logger's level to `s_GlobalLevel` and flush-on-error.

The 14 predefined categories (Core, Graphics, Renderer, Shader, Input, FileSystem,
Window, ImGui, Demo, Serialization, Assert, Ensure, Error, Crash) serve double
duty: they are both the logger names used at runtime and the validation set for
the debug console's command parser (`IsKnownCategory()`).

### 3.4 Log Macros and Compile-Time Level Stripping

All macros use a shared internal macro `RTRLAB_LOG_IMPL_` that wraps the call in a
`do { ... } while (0)` block:

```cpp
#define RTRLAB_LOG_IMPL_(category, level, ...)                     \
    do                                                             \
    {                                                              \
        auto _logger = ::Diagnostics::Logger::GetLogger(category); \
        if (_logger)                                               \
            _logger->level(__VA_ARGS__);                           \
    } while (0)
```

`RTRLAB_LOG_MIN_LEVEL` controls compile-time stripping. Macros below the threshold
expand to `((void)0)` -- no format string in the binary, no function call, no
argument evaluation. The threshold is set per CMake config:

| Config | Min Level | Strips |
|--------|-----------|--------|
| Debug | 0 (Trace) | Nothing |
| RelWithDebInfo | 1 (Debug) | Trace |
| Release | 2 (Info) | Trace + Debug |

A fallback in `LogMacros.h` checks `NDEBUG` for builds that don't set the define.

### 3.5 Flood Prevention Internals (ONCE / THROTTLE / COND)

**ONCE**: Uses a `static std::atomic<bool>` per call site. `compare_exchange_strong`
with relaxed ordering ensures exactly one thread wins. Relaxed ordering is correct
here because the worst case of a spurious CAS failure (impossible with `strong`,
but the relaxed ordering means the `true` write may not be immediately visible to
other cores) is one extra log message, not a safety violation.

**THROTTLE**: Uses a `static std::atomic<double>` holding the last log timestamp.
A `compare_exchange_weak` loop loads the previous timestamp, checks if the interval
has elapsed, and atomically claims the right to log by writing the current timestamp.
The `weak` variant is used inside a loop because spurious failures are retried
automatically. `GetMonotonicSeconds()` wraps `steady_clock` to avoid including
`<chrono>` in the macro header.

**COND**: Simple `if (condition) [[unlikely]]` guard. The `[[unlikely]]` attribute
hints the compiler to arrange the hot path (no logging) for fall-through and branch
to the logging path.

Both ONCE and THROTTLE are currently provided for WARN and ERROR levels only.
TRACE/DEBUG/INFO variants are not implemented -- this is intentional since flood
prevention is mainly needed for messages that indicate problems.

### 3.6 Sink Architecture

All four sinks are created at `Logger::Init` and shared via `s_Sinks` vector.
Every new logger created by `GetLogger` receives the same sink vector. Formatters
are set per-sink, not per-logger.

| Sink | Mutex Type | Special Behavior |
|------|-----------|-----------------|
| `stdout_color_sink_mt` | Internal (spdlog) | Color-coded by level |
| `rotating_file_sink_mt` | Internal (spdlog) | 1MB per file, 3 rotations, custom `%@` frame flag |
| `ImGuiConsoleSink` | `base_sink<std::mutex>` + internal `m_BufferMutex` | Ring buffer (1024 entries) |
| `JsonLineSink` | `base_sink<std::mutex>` | Enable/disable by opening/closing file |

**Flush strategy**:
- Periodic flush every 3s (`spdlog::flush_every`).
- Immediate flush on `error` and `critical` (`logger->flush_on(spdlog::level::err)`).
- Forced flush in crash handler via `spdlog::shutdown()`.

### 3.7 Frame Number Injection

`FrameFormatter.h` defines:
- `g_FrameNumber`: a `std::atomic<uint64_t>`, incremented by `IncrementFrameNumber()`.
- `FrameFlag`: a custom spdlog flag formatter registered as `%@`. On format, it reads
  the counter with relaxed ordering and outputs a zero-padded 8-digit number.

The counter is incremented inside `Application::RenderFrame()`, not in the main loop
tick. This accurately counts rendered frames: it does not increment when the window is
minimized (no render) and does not double-count during live-resize refresh callbacks.

### 3.8 JSON Lines Sink and Control Protocol

`JsonLineSink` inherits `base_sink<std::mutex>`. Its lifecycle is controlled by a
state machine:

```
[Disabled]  --Enable(path)--> [Enabled]  --RequestDisable()--> [PendingDisable]
                                   |                                  |
                                   |--RequestReopen(newPath)-->  [PendingReopen]
                                   |                                  |
                                   |        <---flush_()---           |
                                   |        (close old, open new)     |
                                                                      |
                                   <---flush_()---                    |
                                   (close file)                       |
```

The key insight: `Enable`/`Disable`/`RequestDisable`/`RequestReopen` all lock
`base_sink::mutex_`, which is the same mutex the async worker acquires before
calling `sink_it_()`. This provides correct synchronization without additional
locking.

The `WaitForControl(generation)` mechanism allows callers (like `Logger::EnableJsonSink`)
to block until a deferred operation completes. Each deferred request gets a monotonically
increasing generation number. `flush_()` checks for pending operations and completes
them after flushing current data. It then notifies waiters via `m_ControlCv`.

**JSON escaping**: `WriteJsonString` handles `"`, `\`, `\n`, `\r`, `\t`, and control
characters below 0x20 (output as `\uXXXX`). This covers the JSON spec requirements.

**Timestamp format**: Uses `localtime_s`/`localtime_r` (platform-specific thread-safe
variants) and `std::put_time` for ISO 8601 output with millisecond precision.

### 3.9 ImGui Console Sink

`ImGuiConsoleSink` maintains a `std::deque<ConsoleLogEntry>` ring buffer capped at
1024 entries. Each entry stores: Level, Category, Message, Timestamp (all as strings).

It inherits `base_sink<std::mutex>` but also has its own `m_BufferMutex`. The
`base_sink::mutex_` is acquired by the spdlog framework before calling `sink_it_()`.
The internal `m_BufferMutex` is acquired inside `sink_it_()`, `GetEntries()`, and
`Clear()`. This double-lock design exists so that the ImGui render thread can read
entries (`GetEntries`) without blocking the async logger from writing to other sinks.

`GetEntries()` returns a full copy of the deque as a `std::vector`. This ensures
thread safety but means the GUI thread allocates O(N * 4 strings) per frame.

---

## 4. Assertion Subsystem Internals

### 4.1 Three-Tier Assertion Model

| Macro | Severity | Terminates? | Reports Once? | Returns Value? |
|-------|----------|-------------|---------------|----------------|
| `RTRLAB_ASSERT` | Fatal | Yes | N/A | No |
| `RTRLAB_ASSERT_MSG` | Fatal | Yes | N/A | No |
| `RTRLAB_ASSERTF` | Fatal | Yes | N/A | No |
| `RTRLAB_VERIFY` | Fatal | Yes | N/A | Yes (expr result) |
| `RTRLAB_ENSURE` | Non-fatal | No | Yes (per call-site) | Yes (bool) |
| `RTRLAB_ENSURE_MSG` | Non-fatal | No | Yes | Yes (bool) |
| `RTRLAB_ENSUREF` | Non-fatal | No | Yes | Yes (bool) |

All assertions are **always active** -- they are never stripped by `NDEBUG` or build
configuration. The cost is negligible: the condition check is a branch prediction hint
(`[[unlikely]]`) and the failure path only runs on actual bugs.

### 4.2 Assertion Failure Flow

**ASSERT path**:
1. Condition fails (`[[unlikely]]` branch)
2. `OnAssertionFailed()` is called with expression string, file, line, function, optional message
3. Captures callstack via `CaptureCallstack(framesToSkip=2)` (skips OnAssertionFailed + macro wrapper)
4. Logs at CRITICAL level to the "Assert" category
5. Falls back to `fprintf(stderr)` if logger is unavailable
6. Calls `Logger::Flush()` to ensure the message reaches disk
7. `BreakInDebuggerIfAttached()` -- halts at the assertion site if a debugger is present
8. `CrashHandler::FatalError()` -- writes crash artifacts and terminates

**ENSURE path**:
1. Condition fails (no `[[unlikely]]` attribute)
2. Per-call-site `static std::atomic<bool>` ensures only the first failure is reported
   (uses `compare_exchange_strong` with relaxed ordering, same pattern as LOG_ONCE)
3. If first failure: `OnEnsureFailed()` logs at ERROR level to the "Ensure" category,
   captures and logs callstack, then calls `Logger::Flush()`
4. Returns `false` to the caller -- the caller can use this to take a recovery path

**VERIFY**: Wraps the expression in an immediately-invoked lambda (`[&]()`) so it
can both evaluate the expression and check the result. The lambda captures by
reference so the expression's side effects are visible to the caller. Returns the
expression's original value (not just bool).

### 4.3 Error Macros (ERR_FAIL_COND)

Four variants, all following the same pattern:

```cpp
if (condition) [[unlikely]]
{
    LOG_ERROR_CAT(category, "file:line: condition 'expr' failed [: message]");
    return [retval];
}
```

These are Godot-inspired early-return macros for recoverable errors. They log at
ERROR level and return from the enclosing function. No exceptions, no callstack
capture -- lighter weight than ENSURE for expected failure paths.

---

## 5. Crash Handler Internals

### 5.1 Platform Registration

**Windows** (`Win32CrashHandler.cpp`):
- `SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX)` -- suppresses the
  Windows "program has stopped working" dialog.
- `SetUnhandledExceptionFilter` -- registers `UnhandledExceptionHandler` as the last-chance
  SEH handler.
- Idempotent via `std::atomic_bool` + `compare_exchange_strong`.

**POSIX** (`PosixCrashHandler.cpp`):
- Allocates a 64KB alternate signal stack (`sigaltstack`) so the handler can run even
  when the main stack has overflowed.
- Warms up `backtrace()` by calling it once, forcing libgcc to load its internal
  data structures before a fatal signal arrives.
- Registers `SA_SIGINFO | SA_RESETHAND | SA_ONSTACK` handlers for SIGSEGV, SIGABRT,
  SIGFPE, SIGBUS, SIGILL via `sigaction`.
- `SA_RESETHAND` ensures the handler runs at most once -- if the handler itself crashes,
  the default action (core dump) takes over.

### 5.2 Crash Sequence and Artifact Generation

The cross-platform `HandleCrash()` function:

1. `g_HandlingCrash.test_and_set()` -- atomic flag prevents re-entrant crash handling.
   If already set (recursive crash), immediately calls `TerminateAfterCrash()`.
2. Captures callstack if not provided.
3. Generates timestamp string.
4. **Windows only**: Writes minidump via `MiniDumpWriteDump` (MiniDumpNormal level).
5. Logs at CRITICAL level to the "Crash" category (with fallback to stderr).
6. Writes crash summary `.txt` file containing: timestamp, reason, minidump path,
   callstack, and the last 80 lines of the text log.
7. Calls `Logger::Flush()`.
8. `TerminateAfterCrash()`: flushes, then `TerminateProcess` (Windows) or `_exit(1)` (POSIX).

Crash artifacts are written to `{CrashDirectory}/RTRLab_{YYYYMMDD_HHMMSS}.{ext}`.
The crash directory is resolved via `FileSystem::ResolveWritePath("/Saved/logs/crashes")`.

### 5.3 POSIX Signal Safety

The POSIX signal handler (`PosixSignalHandler`) is designed for async-signal-safety:

- Uses only `write()`, `fsync()`, `close()`, `open()`, `backtrace()`,
  `backtrace_symbols_fd()`, `kill()`, `_exit()` -- all async-signal-safe.
- String formatting uses hand-rolled functions (`AppendLiteral`, `AppendUnsigned`,
  `AppendPointer`) that operate on a fixed stack buffer with no heap allocation.
- Does NOT use the normal logger path (which involves `std::string`, `spdlog`, mutexes).
- Writes to both stderr and a crash file simultaneously.
- After writing, re-raises the signal with `SIG_DFL` to get the default behavior
  (core dump for SIGSEGV etc.), then calls `_exit(128 + signal)` as a backstop.

The crash file path (`g_SignalCrashPath`) is pre-computed at `Init()` time to avoid
any filesystem resolution during signal handling.

### 5.4 Callstack Capture

**Windows** (`Win32Callstack.cpp`):
- `CaptureStackBackTrace` -- Windows API for fast stack walking (max 62 frames).
- `DbgHelp` symbolization: `SymInitialize` (once, lazy, mutex-protected),
  `SymFromAddr` for function names, `SymGetModuleInfo64` for module names.
- Output format: `[N] module.exe!FunctionName + 0xOffset (0xAddress)`

**POSIX** (`PosixCallstack.cpp`):
- `backtrace()` for frame addresses.
- `dladdr()` for symbol and module lookup.
- `abi::__cxa_demangle()` for C++ name demangling.
- Output format: `[N] libmodule.so!Namespace::Function + 0xOffset (0xAddress)`

Both implementations skip internal frames via `framesToSkip` parameter (typically 2:
the `CaptureCallstack` function itself plus the assertion/crash macro wrapper).

### 5.5 Debugger Integration

`Debugger.h` provides two functions:

**`IsDebuggerAttached()`**: Platform-specific detection:
- Windows: `::IsDebuggerPresent()` (Win32 API)
- macOS: `sysctl` with `KERN_PROC_PID`, checks `P_TRACED` flag
- Linux: reads `/proc/self/status`, parses `TracerPid:` field

**`BreakInDebuggerIfAttached()`**: If a debugger is attached, triggers a breakpoint:
- Windows: `__debugbreak()` (compiler intrinsic)
- POSIX: `std::raise(SIGTRAP)`

This is called from `OnAssertionFailed` before `CrashHandler::FatalError`, so the
developer's debugger halts at the assertion site rather than deep in the crash handler.

---

## 6. Debug Console (ConsolePanel)

The `ConsolePanel` is an ImGui panel that provides live log viewing and runtime commands.

**Rendering** (`OnImGuiRender`):
- Menu bar: Clear, Flush, Auto-scroll toggle, Category combo, Level combo.
- Log region: Reads all entries from `ImGuiConsoleSink::GetEntries()` (full copy),
  filters by category and level, color-codes by level, formats into a fixed buffer.
- Command input: Single-line text field with Enter-to-execute.

**Commands**:
- `log.level <category|*> <level>` -- per-category or global level control.
- `log.filter <category|*>` -- display filter (does not affect logging, only display).
- `log.clear` -- clears the ImGui ring buffer.
- `log.flush` -- forces all sinks to flush.
- `log.json <on|off|status> [path]` -- controls the JSON Lines sink.

**Category validation**: `CanAddressCategoryFromConsole()` accepts a category if it is
in the `KnownCategories` array OR has already been created as a logger. This prevents
typos from silently creating empty loggers.

**Level parsing**: Uses `magic_enum::enum_cast` for spdlog level names, with manual
fallbacks for "error" (spdlog uses "err") and "warning" (spdlog uses "warn").

---

## 7. Threading and Synchronization Map

| Resource | Lock | Held By | Notes |
|----------|------|---------|-------|
| `g_Initialized`, `s_Sinks`, `s_*` statics | `g_LoggerMutex` | Logger public API | Protects all Logger state |
| spdlog logger registry | spdlog internal | `GetLogger`, `SetGlobalLevel` | Via `spdlog::get`, `spdlog::register_logger` |
| Async message queue | Lock-free MPSC | Caller threads (enqueue), worker (dequeue) | 8192 slots, `overrun_oldest` |
| `ImGuiConsoleSink::m_Entries` | `m_BufferMutex` | `sink_it_`, `GetEntries`, `Clear` | Separate from `base_sink::mutex_` |
| `JsonLineSink` file + state | `base_sink::mutex_` | `sink_it_`, `Enable`, `Disable`, `Request*` | Single mutex for all operations |
| `g_SymbolMutex` | `std::mutex` | `CaptureCallstack` (Win32) | Protects `SymInitialize`/`SymFromAddr` |
| `g_HandlingCrash` | `std::atomic_flag` | `HandleCrash` | Prevents re-entrant crash handling |
| LOG_ONCE flags | `static std::atomic<bool>` per call site | Any thread | CAS with relaxed ordering |
| LOG_THROTTLE timestamps | `static std::atomic<double>` per call site | Any thread | CAS loop with relaxed ordering |
| Frame counter | `std::atomic<uint64_t>` | Render thread (write), worker (read) | Relaxed ordering both sides |

---

## 8. Comparison with Industry Engines

### 8.1 vs Unreal Engine 5

| Aspect | RTRLab | Unreal Engine 5 |
|--------|--------|-----------------|
| Category declaration | `constexpr const char*` in any header | `DECLARE_LOG_CATEGORY_EXTERN` macro + `.cpp` definition |
| Async backend | spdlog thread pool (MPSC, 1 worker) | Custom `FOutputDevice` hierarchy, threaded |
| Compile-time stripping | `RTRLAB_LOG_MIN_LEVEL` per CMake config | `COMPILE_*_VERBOSITY` per category |
| Assertions | ASSERT/VERIFY/ENSURE (3 tiers) | check/verify/ensure (3 tiers, same model) |
| Crash handling | SEH + MiniDumpNormal | SEH + MiniDumpWithFullMemory + CrashReportClient upload |
| Flood prevention | ONCE/THROTTLE/COND macros | `UE_LOG` frequency limiting, `LogOnce` |
| JSON output | JsonLineSink (JSON Lines) | Structured log channels (JSON, CSV) |
| Error returns | `ERR_FAIL_COND_*` (Godot-inspired) | Custom result types, `ensure` as soft check |

RTRLab's assertion model is directly inspired by Unreal's. The main difference is
scale: Unreal's crash pipeline includes a separate CrashReportClient process, full
memory dumps, and server-side symbolication. RTRLab's approach is appropriate for a
research project -- `MiniDumpNormal` captures thread stacks and loaded modules
without the complexity of a symbol server.

### 8.2 vs Godot Engine

| Aspect | RTRLab | Godot |
|--------|--------|-------|
| Error handling | `ERR_FAIL_COND_*_CAT` (category-aware) | `ERR_FAIL_COND_V` (no categories) |
| Async logging | spdlog async logger | Synchronous (file I/O on caller thread) |
| Compile-time stripping | Full macro elimination | No compile-time stripping |
| Structured output | JSON Lines sink | Plain text only |
| Callstack capture | Platform-native (DbgHelp/backtrace) | Platform-native |
| Crash handler | SEH + sigaction | SEH + sigaction (similar approach) |

RTRLab borrows Godot's `ERR_FAIL_COND_*` pattern but adds category awareness.
The async logging pipeline is a significant improvement over Godot's synchronous
approach for a real-time rendering application.

### 8.3 vs CryEngine / O3DE

| Aspect | RTRLab | CryEngine / O3DE |
|--------|--------|-----------------|
| Logging | Category + level | Subsystem ID + verbosity |
| Assertions | 3 tiers, always active | `CRY_ASSERT` (always active) |
| Exceptions | None in engine code | None in engine code |
| Crash reporting | Local artifacts only | Full crash reporter with server upload |
| Debug console | ImGui panel with commands | In-engine console with CVar integration |

CryEngine integrates logging more deeply with its CVar system. RTRLab's debug console
commands provide similar runtime control but without the CVar infrastructure. Both
engines share the philosophy of always-active assertions and no C++ exceptions.
