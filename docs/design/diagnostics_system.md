# Diagnostics System

A unified logging, assertion, and error-handling framework for the RT Rendering Lab
engine. Replaces the original minimal Logger + ad-hoc exception pattern with a
production-grade diagnostics infrastructure aligned with modern game engine practices.

> **Design Philosophy**: Errors are classified by severity and recoverability.
> The engine never silently swallows failures - every error path produces a diagnostic
> record. Assertions are *always active* (not stripped in Release). Exceptions are
> eliminated from the hot path; error codes and early-return macros handle recoverable
> failures. Logging is asynchronous and context-rich - every message carries frame
> number, thread ID, and category, enabling post-mortem analysis and real-time filtering.

---

## Table of Contents

- [1. Motivation](#1-motivation)
- [2. Architecture Overview](#2-architecture-overview)
- [3. Logging](#3-logging)
  - [3.1 Logger Initialization and Lifecycle](#31-logger-initialization-and-lifecycle)
  - [3.2 Category System](#32-category-system)
  - [3.3 Log Macros](#33-log-macros)
  - [3.4 Flood Prevention (Once / Throttle / Cond)](#34-flood-prevention-once--throttle--cond)
  - [3.5 Sinks](#35-sinks)
  - [3.6 Asynchronous Logging](#36-asynchronous-logging)
  - [3.7 Frame Number and Thread ID](#37-frame-number-and-thread-id)
  - [3.8 Runtime Level Control](#38-runtime-level-control)
  - [3.9 Compile-Time Level Stripping](#39-compile-time-level-stripping)
  - [3.10 JSON Lines Sink](#310-json-lines-sink)
- [4. Assertions](#4-assertions)
- [5. Error Handling](#5-error-handling)
- [6. Crash Handler](#6-crash-handler)
- [7. Debug Console](#7-debug-console)
- [8. File Layout](#8-file-layout)
- [9. Key Design Decisions](#9-key-design-decisions)
- [Appendix A: Industry Survey](#appendix-a-industry-survey)
- [Appendix B: Alternatives Considered](#appendix-b-alternatives-considered)

---

## 1. Motivation

The original diagnostics infrastructure consisted of a single spdlog console sink with
no categories, standard `assert()` stripped in Release, `LOG_ERROR` + `throw` for error
handling, and no crash handler. This left crashes undiagnosable, errors inconsistently
handled, and log output unstructured.

Modern engines (Unreal, Godot, CryEngine) share a common pattern: no C++ exceptions in
engine code, category-based logging with file persistence, always-active assertions with
callstack capture, and crash handlers that preserve diagnostics before termination. This
system follows that pattern.

**What the system provides**:

- **Named categories** with independently controllable verbosity per subsystem.
- **Rotating file logs** - crashes are always diagnosable after the fact.
- **Frame number, thread ID, and timestamp** on every message for correlation with
  rendering events and GPU captures.
- **Once / Throttle / Conditional** variants to prevent log flooding from per-frame code.
- **Asynchronous logging** that moves file I/O off the render thread.
- **Compile-time level stripping** that removes Trace/Debug logs from Release binaries entirely.
- **Structured JSON Lines** output for external tooling and CI analysis.
- **Always-active assertions** with callstack capture even in Release.
- **Early-return macros** instead of exceptions for recoverable errors.
- **Cross-platform crash handler** (Windows SEH + POSIX signals) that flushes logs and
  writes crash artifacts before termination.

---

## 2. Architecture Overview

```
   Diagnostic Sources                 Diagnostics Core                       Outputs
 ┌─────────────────┐               ┌─────────────────────┐            ┌──────────────────┐
 │ LOG_*_CAT(...)   │──────────────▸│                     │──────────▸ │ Console Sink     │
 │ LOG_*_ONCE(...)  │──────────────▸│  Async Logger Pool  │──────────▸ │ File Sink        │
 │ LOG_*_THROTTLE() │──────────────▸│  (spdlog async +    │──────────▸ │ ImGui Ring Sink  │
 │ LOG_*_COND(...)  │──────────────▸│   categories +      │──────────▸ │ JSON Lines Sink  │
 │ RTRLAB_ASSERT    │──────────────▸│   frame formatter)  │            └──────────────────┘
 │ RTRLAB_VERIFY    │──────────────▸│                     │
 │ RTRLAB_ENSURE    │──────────────▸│  ┌───────────────┐  │
 │ ERR_FAIL_COND_*  │──────────────▸│  │ Thread Pool   │  │
 └─────────────────┘               │  │ (MPSC queue)  │  │
                                   │  └───────────────┘  │
        Context injection:         └────────┬────────────┘
        • Timestamp (%T.%e)                 │
        • Thread ID (%t)           ┌────────▼────────────┐
        • Frame Number (%@)        │   Crash Handler     │
        • Category (%n)            │   (SEH / sigaction) │
        • Level (%l)               │   → flush all sinks │
                                   │   → write .dmp      │
  Compile-time stripping:          │   → crash summary   │
  RTRLAB_LOG_MIN_LEVEL             │   → terminate       │
  removes low-severity macros      └─────────────────────┘
```

**Data flow**:

- **Normal path**: Source → log macro → async spdlog logger (category) → thread pool → sinks
- **Once/Throttle path**: Source → per-call-site atomic guard → suppress or pass through → normal path
- **Assertion path**: Condition fails → log with `__FILE__`, `__LINE__`, callstack → assertion handler
- **Error return path**: Condition fails → log → return error value (no exception)
- **Fatal path**: Crash handler → `spdlog::shutdown()` (synchronous drain) → crash artifact → terminate
- **Compile-time path**: `RTRLAB_LOG_MIN_LEVEL` elides macros to `((void)0)` - zero runtime cost

---

## 3. Logging

### 3.1 Logger Initialization and Lifecycle

`Diagnostics::Logger` is a static class that owns the shared sink set and manages
per-category spdlog loggers. It is initialized once at application startup and shut down
at destruction.

**Startup** (`Logger::Init`):
1. Creates the four sinks: console, rotating file, ImGui ring buffer, and JSON Lines (disabled).
2. Initializes the spdlog thread pool (8192-slot MPSC queue, 1 worker thread).
3. Registers the "Core" category as an async logger.
4. Starts periodic flush every 3 seconds.

**Shutdown** (`Logger::Shutdown`):
1. Calls `spdlog::shutdown()` which synchronously drains the async queue, flushes all
   sinks, stops the thread pool and periodic flusher, then drops all loggers.
2. Disables the JSON sink (closes the file).
3. Releases all sink references and resets state.

The ordering is critical: `spdlog::shutdown()` must complete before any sink files are
closed or references released, because the async worker thread may still be processing
queued messages. An earlier design called `drop_all()` instead, but that does not stop
the thread pool or periodic flusher - `spdlog::shutdown()` is the correct full teardown.

**Lazy logger creation**: `GetLogger(category)` checks the spdlog registry first
(`spdlog::get`). On miss, it creates a new async logger sharing the same sink vector
and registers it. This means subsystems don't need to pre-register categories - the
first `LOG_*_CAT(LogCategory::Shader, ...)` call creates the "Shader" logger on demand.

### 3.2 Category System

Categories are `constexpr const char*` strings, not an enum. This is intentional -
subsystems can define additional categories in their own headers without modifying a
central header. Each category maps 1:1 to a spdlog logger instance.

Predefined categories (`LogCategories.h`): Core, Graphics, Renderer, Shader, Input,
FileSystem, Window, ImGui, Demo, Serialization, Assert, Ensure, Error, Crash.

A `KnownCategories` array and `IsKnownCategory()` function provide compile-time
validation for the debug console's command parser, preventing misspelled category names
from silently creating empty loggers. Dynamic (non-predefined) categories that have
already been used are also accepted - this is checked via `Logger::HasLogger()`.

### 3.3 Log Macros

All macros are defined in `LogMacros.h`. The `_CAT` variants take a category as the
first argument. Legacy macros (no category) route to `LogCategory::Core`.

```
LOG_TRACE_CAT / LOG_DEBUG_CAT / LOG_INFO_CAT / LOG_WARN_CAT / LOG_ERROR_CAT / LOG_CRITICAL_CAT
LOG_TRACE      / LOG_DEBUG      / LOG_INFO      / LOG_WARN      / LOG_ERROR      / LOG_CRITICAL
```

Each macro expands to a `do { ... } while (0)` block that calls `Logger::GetLogger(category)`
and invokes the corresponding spdlog level method. The `RTRLAB_LOG_IMPL_` internal macro
eliminates duplication across all six levels.

### 3.4 Flood Prevention (Once / Throttle / Cond)

High-frequency code paths (per-frame updates, per-draw-call validation) can produce
thousands of identical messages per second. Three macro families prevent this:

**`LOG_*_ONCE_CAT`** - Logs only on the first occurrence at each call site. Uses a
`static std::atomic<bool>` with `compare_exchange_strong` to guarantee exactly one
thread wins the race. Relaxed memory ordering suffices because the worst case of a
spurious CAS failure is one missed log, not a safety violation.

**`LOG_*_THROTTLE_CAT`** - Logs at most once per N seconds per call site. Uses a
`static std::atomic<double>` holding the last log timestamp and a CAS loop
(`compare_exchange_weak`) to atomically claim the right to log. The time source is
`GetMonotonicSeconds()`, which wraps `std::chrono::steady_clock` to avoid
`#include <chrono>` in the macro header.

**`LOG_*_COND_CAT`** - Logs only when a boolean condition is true. Equivalent to
Unreal's `UE_CLOG`. Marked `[[unlikely]]` to hint the compiler that the logging path
is not the common case.

**Thread safety design**: An earlier implementation used non-atomic `static bool` /
`static double`, which compiled but constituted a data race under the C++ memory model.
Even though the practical impact was merely occasional duplicate output, the fix to
`std::atomic` + CAS eliminates the undefined behavior entirely. The CAS approach also
provides stronger semantic guarantees: ONCE truly fires once (not "approximately once"),
and THROTTLE truly allows one winner per interval.

### 3.5 Sinks

Four sinks are created at `Logger::Init` time and shared by all loggers:

| Sink | Type | Pattern | Purpose |
|------|------|---------|---------|
| Console | `stdout_color_sink_mt` | `[%T] [%n] [%^%l%$] %v` | Interactive debugging with colored output |
| File | `rotating_file_sink_mt` | `[%Y-%m-%d %T.%e] [%t] [F%@] [%n] [%l] %v` | Persistent log with full context (1 MB per file, 3 rotations) |
| ImGui | `ImGuiConsoleSink` | (structured fields) | In-engine debug console ring buffer (1024 entries) |
| JSON | `JsonLineSink` | (JSON objects) | Machine-readable structured output (disabled by default) |

Formatters are set per-sink, not per-logger. The file sink uses a custom
`spdlog::pattern_formatter` with the `FrameFlag` registered as the `%@` flag.

**Flush strategy**:
- Periodic flush every 3 seconds via `spdlog::flush_every`.
- Immediate flush on `error` and `critical` levels (configured per-logger via `flush_on`).
- Forced flush in the crash handler via `spdlog::shutdown()`.

### 3.6 Asynchronous Logging

All loggers are `spdlog::async_logger` instances backed by a shared thread pool.

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Queue size | 8192 | ~64 KB memory. Handles bursts before dropping. |
| Worker threads | 1 | File I/O is sequential; multiple workers add contention without benefit. |
| Overflow policy | `overrun_oldest` | Prefer losing old messages over blocking the render thread. |

The caller thread performs only a lock-free enqueue (~10 ns). The worker thread formats
the message and writes to all sinks. Within a single category, message ordering is
preserved. Across categories, slight reordering is possible.

**Shutdown safety**: `spdlog::shutdown()` synchronously joins the worker thread after
draining the queue. This guarantees all buffered messages reach disk before the process
exits.

**Testing note**: Because `logger->flush()` for an async logger only posts a flush
request to the queue (it does not block until completion), tests that verify sink content
must poll with a timeout rather than assuming immediate availability after `Flush()`.

### 3.7 Frame Number and Thread ID

Every file-sink log line includes the frame number and thread ID:

```
[2026-03-25 14:32:01.234] [12340] [F00004823] [Shader] [error] Compilation failed ...
 ^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^   ^^^^^^^^^   ^^^^^^   ^^^^^
 timestamp                  thread   frame #    category  level
```

**Frame counter**: A `std::atomic<uint64_t>` incremented once per rendered frame in
`Application::RenderFrame()`. The `FrameFlag` custom spdlog formatter reads it with
relaxed ordering and outputs it as a zero-padded 8-digit number.

The counter is incremented inside `RenderFrame()`, not in the main loop tick. This means
it accurately counts rendered frames - it does not increment when the window is minimized
(no render) and does not double-count during live-resize refresh callbacks (which also
call `RenderFrame()`).

**Thread ID**: Built into spdlog via `%t`. No custom code needed.

### 3.8 Runtime Level Control

```cpp
Diagnostics::Logger::SetLevel(LogCategory::Shader, spdlog::level::warn);
Diagnostics::Logger::SetGlobalLevel(spdlog::level::info);
```

Also exposed via the ImGui debug console:

```
> log.level Shader warn
> log.level * info
```

### 3.9 Compile-Time Level Stripping

`RTRLAB_LOG_MIN_LEVEL` controls which macros are compiled in. Macros below the minimum
level expand to `((void)0)` - no format strings in the binary, no runtime cost.

| Level | Name | Default Config |
|-------|------|---------------|
| 0 | Trace | Debug |
| 1 | Debug | RelWithDebInfo |
| 2 | Info | Release |
| 3 | Warn | - |
| 4 | Error | - |
| 5 | Critical | - |
| 6 | Off | - |

All six `LOG_*_CAT` macros are guarded. The CMake configuration:

```cmake
target_compile_definitions(RTRLabCore PUBLIC
    $<$<CONFIG:Debug>:RTRLAB_LOG_MIN_LEVEL=0>
    $<$<CONFIG:Release>:RTRLAB_LOG_MIN_LEVEL=2>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_LOG_MIN_LEVEL=1>
)
```

A fallback default is provided in `LogMacros.h` based on `NDEBUG` for builds that don't
set the define explicitly.

### 3.10 JSON Lines Sink

The `JsonLineSink` writes one JSON object per log message for machine-readable output:

```json
{"ts":"2026-03-25T14:32:01.234","frame":4823,"tid":12340,"cat":"Shader","lvl":"error","msg":"Compilation failed (PBR.frag)"}
```

| Field | Type | Description |
|-------|------|-------------|
| `ts` | string (ISO 8601) | Timestamp with millisecond precision |
| `frame` | integer | Frame number |
| `tid` | integer | Thread ID |
| `cat` | string | Log category |
| `lvl` | string | Level name |
| `msg` | string | Formatted message (JSON-escaped) |

**Always-registered, enable-on-demand design**: The sink is created at `Logger::Init()`
in a disabled state (no file open) and included in the shared sink vector from the start.
`EnableJsonSink(path)` opens the file; `DisableJsonSink()` requests closure after a
flush barrier. This avoids the thread-safety hazard of mutating a live logger's sink
list at runtime - the async worker thread iterates the sink vector without external
locking, so adding/removing sinks while it runs would be a data race.

The `Enable` / `Disable` / `RequestDisable` / `RequestReopen` methods all lock the
`base_sink<std::mutex>::mutex_`, which is the same mutex the async worker acquires
before calling `sink_it_()`. This provides correct synchronization without any
additional locking mechanism.

**Why JSON Lines**: One JSON object per line, no wrapping array. Can be appended without
reading the full file. Survives truncation (each line is independently parseable).
Compatible with `jq`:

```bash
cat saved/logs/RTRLab.jsonl | jq 'select(.cat=="Shader" and .lvl=="error")'
```

---

## 4. Assertions

Three tiers, inspired by Unreal's `check` / `verify` / `ensure`:

**`RTRLAB_ASSERT(condition)`** / **`RTRLAB_ASSERTF(condition, fmt, ...)`**
Hard assertion. Active in ALL builds (Debug and Release). On failure: logs at CRITICAL
level with file/line, captures callstack, breaks into attached debugger, then terminates.
Use for invariants that must hold - violation means a bug that corrupts state.

**`RTRLAB_VERIFY(expr)`**
Identical to ASSERT, but signals that the expression has side effects and must always
be evaluated.

**`RTRLAB_ENSURE(condition)`** / **`RTRLAB_ENSUREF(condition, fmt, ...)`**
Soft assertion. Does NOT terminate. Logs error + callstack on first occurrence, then
continues execution. Returns `false` when the condition fails, so callers can handle
the failure gracefully. Use for "this shouldn't happen but the engine can survive."

**Callstack capture**: Platform-specific implementations in
`backends/win32/Callstack.cpp` (`CaptureStackBackTrace` + `SymFromAddr`) and
`backends/posix/Callstack.cpp` (`backtrace()` + `dladdr()` + `abi::__cxa_demangle()`).

**Debugger integration**: `Debugger.h` provides cross-platform debugger detection
(`IsDebuggerPresent` on Windows, `sysctl` / `ptrace` on POSIX) and a `DebugBreak()`
trap that halts at the assertion site rather than deep inside the diagnostics code.

---

## 5. Error Handling

Recoverable errors use early-return macros inspired by Godot's `ERR_FAIL_*` family:

```cpp
ERR_FAIL_COND_CAT(category, condition)              // log + return void
ERR_FAIL_COND_V_CAT(category, condition, retval)     // log + return value
ERR_FAIL_COND_MSG_CAT(category, condition, message)   // log with message + return void
ERR_FAIL_COND_V_MSG_CAT(category, condition, retval, message)  // log with message + return value
```

Non-`_CAT` convenience aliases route to `LogCategory::Error`.

**Error classification**:

| Severity | Response | Macro |
|----------|----------|-------|
| Warning | Log, continue | `LOG_WARN_CAT(...)` |
| Recoverable error | Log, early return | `ERR_FAIL_COND_V(...)` |
| Soft assert | Log + callstack, continue | `RTRLAB_ENSURE(...)` |
| Fatal | Log + callstack, terminate | `RTRLAB_ASSERT(...)` |

**No exceptions in engine code**. Exceptions are only caught at third-party boundaries
(nlohmann/json parsing, `std::filesystem` where error_code overloads aren't available).
Engine-owned control flow uses the macros above.

---

## 6. Crash Handler

`Diagnostics::CrashHandler` registers platform-specific handlers at startup (after
`Logger::Init()`):

- **Windows**: `SetUnhandledExceptionFilter` (SEH) + minidump via `MiniDumpWriteDump`
- **POSIX**: `sigaction` for SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL

**Crash sequence**:

1. Log the fatal error at CRITICAL level
2. Capture and log the callstack
3. `spdlog::shutdown()` - synchronously drain the async queue and flush all sinks to disk
4. Write platform crash artifact (`.dmp` on Windows, `.txt` on POSIX)
5. Terminate (`std::abort()` / `_exit(128 + signal)`)

The POSIX signal handler avoids the normal logger path (which is unsafe in a signal
context) and uses low-level `write()` + `fsync()` to a dedicated crash summary file.

**Dump naming**: `saved/logs/crashes/RTRLab_YYYYMMDD_HHMMSS.dmp`

---

## 7. Debug Console

An ImGui panel (`ConsolePanel`) that displays logs in real time and accepts commands.

**Log display**: Reads from the `ImGuiConsoleSink` ring buffer (1024 entries). Entries
are color-coded by severity (gray=trace, white=info, yellow=warn, red=error,
bold red=critical). A category filter dropdown and level filter provide real-time
filtering. Auto-scroll tracks the bottom of the log.

**Commands**:

| Command | Effect |
|---------|--------|
| `log.level <category> <level>` | Set verbosity for a category |
| `log.level * <level>` | Set global verbosity floor |
| `log.filter <category>` | Show only this category |
| `log.filter *` | Show all categories |
| `log.clear` | Clear the console buffer |
| `log.flush` | Force flush all sinks |
| `log.json on [path]` | Enable JSON Lines sink |
| `log.json off` | Disable JSON Lines sink |

**Category validation**: The command parser accepts a category if it is either in the
`KnownCategories` list or has already been created as a logger (`HasLogger`). This
prevents misspelled categories from silently creating empty loggers, while still
supporting dynamic categories that were registered by subsystem code.

---

## 8. File Layout

```
src/core/diagnostics/
    Logger.h / .cpp              - Async spdlog wrapper, sink management, lifecycle
    LogCategories.h              - Predefined category names + KnownCategories array
    LogMacros.h                  - LOG_*_CAT, _ONCE, _THROTTLE, _COND, compile-time stripping
    FrameFormatter.h / .cpp      - Custom %@ spdlog flag for frame number
    JsonLineSink.h / .cpp        - Structured JSON Lines sink (enable-on-demand)
    ImGuiConsoleSink.h / .cpp    - Ring buffer sink for ImGui console
    Assert.h / .cpp              - RTRLAB_ASSERT / VERIFY / ENSURE
    ErrorMacros.h                - ERR_FAIL_COND_* family
    CrashHandler.h               - Platform-independent crash handler interface
    Callstack.h                  - Platform-independent callstack capture interface
    Debugger.h / .cpp            - Cross-platform debugger detection and trap
    backends/
      win32/
        CrashHandler.cpp         - SEH + MiniDumpWriteDump
        Callstack.cpp            - CaptureStackBackTrace + DbgHelp symbolization
      posix/
        CrashHandler.cpp         - Signal handler + crash summary writer
        Callstack.cpp            - backtrace() + dladdr() + demangling

src/gui/panels/
    ConsolePanel.h / .cpp        - ImGui debug console panel

saved/logs/
    RTRLab.log                   - Rotating text log (human-readable)
    RTRLab.jsonl                 - JSON Lines log (when enabled)
    crashes/
      RTRLab_YYYYMMDD_HHMMSS.dmp  - Minidump (Windows)
      RTRLab_YYYYMMDD_HHMMSS.txt  - Crash summary
```

The `diagnostics/` module lives under `core/` because it is engine infrastructure
depended on by every subsystem. Runtime log paths are resolved through the engine's
saved-data policy: Debug builds write under the source tree's `saved/logs/`, Release
builds write under the platform user-data directory.

---

## 9. Key Design Decisions

### Why `constexpr const char*` categories instead of an enum

Categories are string literals so that subsystems can define new ones in their own
headers without touching a central enum. The `KnownCategories` array provides validation
for the debug console without constraining extensibility.

### Why `overrun_oldest` instead of blocking

The async logger uses `overrun_oldest` overflow policy. If the 8192-slot queue fills up
during a burst, the oldest message is silently dropped rather than blocking the render
thread. A blocked render thread causes visible frame hitches - dropped log messages are
a lesser evil and indicate a burst that should be addressed with THROTTLE macros.

### Why the JSON sink is always registered but disabled

An earlier design added/removed the JSON sink from each logger's sink vector at runtime
via `EnableJsonSink()` / `DisableJsonSink()`. This was a data race: the async worker
thread iterates the sink vector without external locking, and mutating it concurrently
causes undefined behavior. The fix registers the sink once at init and toggles it by
opening/closing its output file, all under the `base_sink` mutex that the async worker
already acquires.

### Why ONCE/THROTTLE use CAS instead of load+store

The initial implementation used separate `atomic::load()` + `atomic::store()` with
relaxed ordering. This eliminated the data race on the static variable but allowed
multiple threads to pass the check simultaneously (TOCTOU). For ONCE, this meant the
message could fire more than once. For THROTTLE, multiple threads could each claim the
interval. `compare_exchange_strong` (ONCE) and a `compare_exchange_weak` loop (THROTTLE)
make these operations truly atomic - exactly one thread wins.

### Why `spdlog::shutdown()` instead of `drop_all()`

`drop_all()` removes loggers from the registry but does not stop the thread pool or the
periodic flusher started by `flush_every()`. `spdlog::shutdown()` does both: it
synchronously drains the async queue, joins the worker thread, and stops the periodic
flush timer. This is the correct full teardown, and it must happen before any sink files
are closed.

### Why assertions are never stripped

Standard `assert()` is removed by `NDEBUG` in Release, which means bugs that corrupt
state manifest as silent data corruption or delayed crashes with no diagnostics.
`RTRLAB_ASSERT` is active in all build configurations. The cost is negligible - the
condition check is a branch prediction hint (`[[unlikely]]`) and the failure path
(logging, callstack, terminate) only runs on actual bugs.

### Why no C++ exceptions in engine code

Exceptions have non-zero overhead even when not thrown (unwind tables, missed
optimizations). More importantly, catch sites in the original code were inconsistent -
some callers caught, some didn't, leading to unpredictable termination. No mainstream
game engine uses exceptions internally. The `ERR_FAIL_COND_*` + `RTRLAB_ASSERT` pattern
gives explicit control over every error path.

---

## Appendix A: Industry Survey

### Unreal Engine 5

**Logging**: `UE_LOG(LogCategory, Verbosity, Format, ...)`. Categories declared with
`DECLARE_LOG_CATEGORY_EXTERN`. Output to console, `Saved/Logs/<Project>.log`, and
Editor Message Log. Per-category runtime verbosity control.

**Assertions**: `check(expr)` (fatal all builds), `verify(expr)` (always evaluated),
`ensure(expr)` (non-fatal, reports once). No C++ exceptions (`-fno-exceptions`).

**Crash handling**: `FPlatformMisc::SetCrashHandler`. SEH on Windows, signals on
Linux/Mac. Minidump + callstack + `CrashReportClient` upload.

### Godot Engine

**Logging**: `print_line`, `WARN_PRINT`, `ERR_PRINT` with file/line. Output to
`user://logs/godot.log` with rotation.

**Error handling**: `ERR_FAIL_COND_V(cond, retval)` family - log + early return.
`CRASH_COND` for truly fatal errors. Almost everything is recoverable. No exceptions.

**Crash handling**: Signal-based on Unix, SEH on Windows. Writes error log with callstack.

### CryEngine / O3DE

**Logging**: `CryLog`, `CryWarning`, `CryFatalError` with verbosity and subsystem IDs.

**Assertions**: `CRY_ASSERT(cond)` - always active, debugger break, crash report in
Release.

**Error handling**: No exceptions. `CryFatalError` terminates with diagnostics.
Recoverable errors return status codes.

---

## Appendix B: Alternatives Considered

### Keep `std::runtime_error` everywhere

Familiar C++ pattern, but exception overhead in hot paths, stack unwinding cost, and
inconsistent catch sites make it unsuitable as the primary error mechanism. Acceptable
at third-party boundaries only.

### Full `Result<T, E>` monadic error type

Type-safe and composable, but significant API churn. Verbose without pattern matching.
Overkill when most errors are "log and skip" or "log and abort". The `ERR_FAIL_COND_V`
+ `RTRLAB_ASSERT` approach achieves 90% of the safety with 10% of the API surface change.

### Third-party crash reporter (Breakpad / Crashpad)

Battle-tested and cross-platform, but heavy dependency (~50K+ LOC) requiring a symbol
server. Overkill for a research project. The current `MiniDumpWriteDump` +
`CaptureStackBackTrace` approach covers diagnostic needs without the complexity.
Can be adopted later if the project grows.
