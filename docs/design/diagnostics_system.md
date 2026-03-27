# Diagnostics System — Design Document

Updated 2026-03-25. Describes a unified logging, assertion, and error-handling framework
that replaces the current minimal Logger + ad-hoc exception pattern with a production-grade
diagnostics infrastructure aligned with modern game engine practices.

> **Design Philosophy**: Errors are classified by severity and recoverability.
> The engine never silently swallows failures — every error path produces a diagnostic record.
> Assertions are *always active* (not stripped in Release). Exceptions are eliminated from the
> hot path; error codes and early-return macros handle recoverable failures.
> Logging is asynchronous and context-rich — every message carries frame number, thread ID,
> and category, enabling post-mortem analysis and real-time filtering.

---

## Table of Contents

- [Diagnostics System — Design Document](#diagnostics-system--design-document)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Architecture Overview](#2-architecture-overview)
  - [3. Layer 0 — Multi-Sink Logger](#3-layer-0--multi-sink-logger)
    - [3.1 Logger Initialization](#31-logger-initialization)
    - [3.2 Category System](#32-category-system)
    - [3.3 Log Macros](#33-log-macros)
    - [3.4 Conditional and Throttled Logging](#34-conditional-and-throttled-logging)
    - [3.5 Sinks](#35-sinks)
    - [3.6 Asynchronous Logging Mode](#36-asynchronous-logging-mode)
    - [3.7 Frame Number and Thread ID Injection](#37-frame-number-and-thread-id-injection)
    - [3.8 Runtime Level Control](#38-runtime-level-control)
    - [3.9 Compile-Time Level Stripping](#39-compile-time-level-stripping)
    - [3.10 Structured Log Format](#310-structured-log-format)
  - [4. Layer 1 — Assertion Framework](#4-layer-1--assertion-framework)
    - [4.1 Assertion Macros](#41-assertion-macros)
    - [4.2 Assertion Handler](#42-assertion-handler)
    - [4.3 Replacing Existing assert() Usage](#43-replacing-existing-assert-usage)
  - [5. Layer 2 — Error Handling Policy](#5-layer-2--error-handling-policy)
    - [5.1 Error Classification](#51-error-classification)
    - [5.2 Error Return Macros](#52-error-return-macros)
    - [5.3 Migration from Exceptions](#53-migration-from-exceptions)
    - [5.4 Where Exceptions Remain](#54-where-exceptions-remain)
  - [6. Layer 3 — Crash Handler](#6-layer-3--crash-handler)
    - [6.1 Windows SEH Handler](#61-windows-seh-handler)
    - [6.2 Cross-Platform Crash Handler Abstraction](#62-cross-platform-crash-handler-abstraction)
    - [6.3 Crash Sequence](#63-crash-sequence)
    - [6.4 Minidump Generation](#64-minidump-generation)
  - [7. Layer 4 — Debug Console Integration](#7-layer-4--debug-console-integration)
  - [8. File Layout](#8-file-layout)
  - [9. Migration Plan](#9-migration-plan)
    - [Phase 1 — Logger upgrade (multi-sink + categories)](#phase-1--logger-upgrade-multi-sink--categories)
    - [Phase 2 — Assertion framework](#phase-2--assertion-framework)
    - [Phase 3 — Error handling policy](#phase-3--error-handling-policy)
    - [Phase 4 — Crash handler](#phase-4--crash-handler)
    - [Phase 5 — Debug console integration](#phase-5--debug-console-integration)
    - [Phase 6 — Extended logging features](#phase-6--extended-logging-features)
  - [10. Appendix A: Industry Survey](#10-appendix-a-industry-survey)
    - [A1. Unreal Engine 5](#a1-unreal-engine-5)
    - [A2. Godot Engine](#a2-godot-engine)
    - [A3. CryEngine / O3DE](#a3-cryengine--o3de)
  - [11. Appendix B: Alternatives Considered](#11-appendix-b-alternatives-considered)
    - [B1. Keep std::runtime\_error everywhere](#b1-keep-stdruntime_error-everywhere)
    - [B2. Full Result\<T, E\> monadic error type](#b2-full-resultt-e-monadic-error-type)
    - [B3. Third-party crash reporter (Breakpad / Crashpad)](#b3-third-party-crash-reporter-breakpad--crashpad)

---

## 1. Motivation

The current diagnostics infrastructure consists of:

| Component | Current State | Problem |
|-----------|---------------|---------|
| Logger | Single spdlog console sink, no categories | Crash = all logs lost; no way to filter subsystem noise |
| Assertions | Standard `assert()` from `<cassert>` | Stripped in Release — bugs manifest as silent corruption or delayed crashes |
| Error handling | `LOG_ERROR` + `throw std::runtime_error` | Exception overhead; no structured recovery; catch sites are inconsistent |
| Crash handling | None | Unhandled exceptions or access violations terminate without diagnostics |

**What modern engines do differently**:

- **Unreal**: `UE_LOG(Category, Verbosity, ...)` with per-category runtime filtering, file + console sinks, `check`/`ensure`/`verify` assertions that survive Release builds, integrated crash reporter with minidump + callstack upload.
- **Godot**: `ERR_FAIL_COND_V` macros that log + early-return instead of throwing, `print_verbose`/`WARN_PRINT`/`ERR_PRINT` with file output, built-in crash handler.
- **Common across all**: No C++ exceptions in engine code. Errors are classified by severity and handled with explicit control flow, not stack unwinding.

**Goal**: A diagnostics system where:
- Every subsystem logs through a **named category** with independently controllable verbosity.
- Logs persist to **rotating files** — crashes are always diagnosable after the fact.
- Every log message carries **frame number, thread ID, and timestamp** for precise correlation with rendering events and GPU captures.
- **Once / Throttle / Conditional** log variants prevent log flooding from per-frame code paths.
- **Asynchronous logging** moves file I/O off the render thread, preventing frame hitches.
- **Compile-time level stripping** removes Trace/Debug logs from Release binaries entirely — no format strings, no runtime cost.
- **Structured JSON Lines** output enables external tooling (log aggregators, CI analysis, `jq` queries).
- Assertions are **always active** and produce actionable diagnostics (callstack, context) even in Release.
- Recoverable errors use **early-return macros** instead of exceptions, giving the engine explicit control over recovery.
- Fatal errors trigger a **cross-platform crash handler** that flushes logs and writes
  platform-appropriate crash artifacts before termination.

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
        • Frame Number (%@frame)   │   Crash Handler     │
        • Category (%n)            │   (SEH / sigaction) │
        • Level (%l)               │   → flush all sinks │
                                   │   → write .dmp      │
  Compile-time stripping:          │   → crash summary   │
  RTRLAB_LOG_MIN_LEVEL             │   → terminate       │
  removes Trace/Info in Release    └─────────────────────┘
```

Data flow:
- **Normal path**: Source → log macro → async spdlog logger (category) → thread pool → sinks (console + file + ImGui + JSON Lines)
- **Once/Throttle path**: Source → static guard (per-call-site) → suppress or pass through → normal path
- **Assertion path**: Condition fails → log with `__FILE__`, `__LINE__`, callstack → assertion handler (break in debugger / crash in Release)
- **Error return path**: Condition fails → log → return error value (no exception)
- **Fatal path**: Unrecoverable error or hardware fault → crash handler → flush async queue → write crash artifact → terminate
- **Compile-time path**: `RTRLAB_LOG_MIN_LEVEL` elides low-severity macros to `((void)0)` — zero runtime cost, no format strings in binary

---

## 3. Layer 0 — Multi-Sink Logger

### 3.1 Logger Initialization

Replace the current single-logger `Logger::Init()` with a system that creates
per-category loggers sharing a common sink set.

```cpp
/// @file core/diagnostics/Logger.h

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "core/Base.h"

namespace Diagnostics {

class Logger
{
public:
    /// Initialize the logging subsystem. Call once at startup, before any LOG_* macro.
    /// Creates shared sinks (console + rotating file) and registers default categories.
    /// The file path should come from FileSystem::GetSavedPath("logs/RTRLab.log").
    static void Init(const std::filesystem::path &logFilePath);

    /// Flush all sinks immediately. Called by the crash handler before termination.
    static void Flush();

    /// Shut down logging. Flushes and drops all loggers.
    static void Shutdown();

    /// Retrieve (or lazily create) a logger for the given category name.
    /// Thread-safe — spdlog's registry handles synchronization.
    static Ref<spdlog::logger> GetLogger(const char *category);

    /// Change the verbosity of a single category at runtime.
    static void SetLevel(const char *category, spdlog::level::level_enum level);

    /// Change the verbosity floor for ALL categories.
    static void SetGlobalLevel(spdlog::level::level_enum level);

private:
    static std::vector<spdlog::sink_ptr> s_Sinks;
    static spdlog::level::level_enum s_GlobalLevel;
};

} // namespace Diagnostics
```

### 3.2 Category System

Each subsystem declares a log category. Categories are lightweight — just a string
name used to retrieve a spdlog logger. No registration macro needed; loggers are
created on first use.

```cpp
/// @file core/diagnostics/LogCategories.h
///
/// Predefined category names. Subsystems may define additional categories
/// in their own headers — the logger creates them lazily.

namespace LogCategory
{
    constexpr const char *Core       = "Core";
    constexpr const char *Graphics   = "Graphics";
    constexpr const char *Renderer   = "Renderer";
    constexpr const char *Shader     = "Shader";
    constexpr const char *Input      = "Input";
    constexpr const char *FileSystem = "FileSystem";
    constexpr const char *Window     = "Window";
    constexpr const char *ImGui      = "ImGui";
    constexpr const char *Demo       = "Demo";
}
```

**Key decisions**:
- Categories are `constexpr const char*`, not an enum — extensible without modifying a central header.
- Each category maps 1:1 to a spdlog logger instance, all sharing the same sink vector.
- `spdlog::get(name)` handles the lookup; on miss, `Logger::GetLogger` creates + registers.
- Log output format includes the category: `[14:32:01] [Graphics] [warn] Texture not found: foo.png`
- `magic_enum` is intentionally not used for categories. Categories stay as strings so
  subsystems can add new names without editing a central enum. `magic_enum` is reserved
  for internal diagnostics enums such as severity, recoverability, sink mode, and
  command-parser level tokens.

### 3.3 Log Macros

The new macros require a category as the first argument. The old convenience macros
(no category) are preserved as aliases to `LogCategory::Core` to avoid a mass rewrite
during migration.

```cpp
/// @file core/diagnostics/LogMacros.h

// --- Category-aware macros (preferred) ---

#define LOG_TRACE_CAT(category, ...)                                       \
    do {                                                                   \
        auto _logger = ::Diagnostics::Logger::GetLogger(category);        \
        if (_logger) _logger->trace(__VA_ARGS__);                         \
    } while (0)

#define LOG_INFO_CAT(category, ...)                                        \
    do {                                                                   \
        auto _logger = ::Diagnostics::Logger::GetLogger(category);        \
        if (_logger) _logger->info(__VA_ARGS__);                          \
    } while (0)

#define LOG_WARN_CAT(category, ...)                                        \
    do {                                                                   \
        auto _logger = ::Diagnostics::Logger::GetLogger(category);        \
        if (_logger) _logger->warn(__VA_ARGS__);                          \
    } while (0)

#define LOG_ERROR_CAT(category, ...)                                       \
    do {                                                                   \
        auto _logger = ::Diagnostics::Logger::GetLogger(category);        \
        if (_logger) _logger->error(__VA_ARGS__);                         \
    } while (0)

#define LOG_CRITICAL_CAT(category, ...)                                    \
    do {                                                                   \
        auto _logger = ::Diagnostics::Logger::GetLogger(category);        \
        if (_logger) _logger->critical(__VA_ARGS__);                      \
    } while (0)

// --- Legacy macros (route to "Core" category for backward compatibility) ---

#define LOG_TRACE(...)    LOG_TRACE_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_INFO(...)     LOG_INFO_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_WARN(...)     LOG_WARN_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_ERROR(...)    LOG_ERROR_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_CRITICAL(...) LOG_CRITICAL_CAT(LogCategory::Core, __VA_ARGS__)
```

**Migration note**: Existing code continues to compile unchanged. New code and
gradually migrated old code use the `_CAT` variants. There is no urgency to
migrate all call sites at once.

### 3.4 Conditional and Throttled Logging

High-frequency code paths (per-frame updates, per-draw-call validation) can produce
thousands of identical log messages per second, drowning useful output and degrading
performance. Modern engines solve this with `ONCE`, `THROTTLE`, and `CLOG` variants.

```cpp
/// @file core/diagnostics/LogMacros.h (continued)

// --- LOG_WARN_ONCE / LOG_ERROR_ONCE ---
// Log the message only on the FIRST occurrence at this call site.
// Uses a static bool to suppress subsequent calls. Thread-safe via function-local static.
//
// Equivalent to: Godot's ERR_PRINT_ONCE, UE's ensureMsgf (report-once semantics)

#define LOG_WARN_ONCE_CAT(category, ...)                                     \
    do {                                                                     \
        static bool _logged = false;                                         \
        if (!_logged) [[unlikely]] {                                         \
            _logged = true;                                                  \
            LOG_WARN_CAT(category, __VA_ARGS__);                             \
        }                                                                    \
    } while (0)

#define LOG_ERROR_ONCE_CAT(category, ...)                                    \
    do {                                                                     \
        static bool _logged = false;                                         \
        if (!_logged) [[unlikely]] {                                         \
            _logged = true;                                                  \
            LOG_ERROR_CAT(category, __VA_ARGS__);                            \
        }                                                                    \
    } while (0)

// Legacy convenience (no category):
#define LOG_WARN_ONCE(...)  LOG_WARN_ONCE_CAT(LogCategory::Core, __VA_ARGS__)
#define LOG_ERROR_ONCE(...) LOG_ERROR_ONCE_CAT(LogCategory::Core, __VA_ARGS__)


// --- LOG_WARN_THROTTLE / LOG_ERROR_THROTTLE ---
// Log the message at most once every N seconds per call site.
// Prevents log flooding from frequently triggered conditions (e.g., missing texture
// referenced every frame, performance warning on every draw call).
//
// Equivalent to: UE's UE_LOG_THROTTLE

#define LOG_WARN_THROTTLE_CAT(category, intervalSeconds, ...)                \
    do {                                                                     \
        static double _lastLogTime = -1e9;                                   \
        double _now = ::Diagnostics::GetMonotonicSeconds();                  \
        if ((_now - _lastLogTime) >= (intervalSeconds)) {                    \
            _lastLogTime = _now;                                             \
            LOG_WARN_CAT(category, __VA_ARGS__);                             \
        }                                                                    \
    } while (0)

#define LOG_ERROR_THROTTLE_CAT(category, intervalSeconds, ...)               \
    do {                                                                     \
        static double _lastLogTime = -1e9;                                   \
        double _now = ::Diagnostics::GetMonotonicSeconds();                  \
        if ((_now - _lastLogTime) >= (intervalSeconds)) {                    \
            _lastLogTime = _now;                                             \
            LOG_ERROR_CAT(category, __VA_ARGS__);                            \
        }                                                                    \
    } while (0)

// Legacy convenience:
#define LOG_WARN_THROTTLE(interval, ...)  LOG_WARN_THROTTLE_CAT(LogCategory::Core, interval, __VA_ARGS__)
#define LOG_ERROR_THROTTLE(interval, ...) LOG_ERROR_THROTTLE_CAT(LogCategory::Core, interval, __VA_ARGS__)


// --- LOG_COND / Conditional logging ---
// Log only when a boolean condition is true. Avoids if-blocks at call sites.
//
// Equivalent to: UE's UE_CLOG

#define LOG_WARN_COND_CAT(condition, category, ...)                          \
    do {                                                                     \
        if (condition) [[unlikely]] {                                        \
            LOG_WARN_CAT(category, __VA_ARGS__);                             \
        }                                                                    \
    } while (0)

#define LOG_ERROR_COND_CAT(condition, category, ...)                         \
    do {                                                                     \
        if (condition) [[unlikely]] {                                        \
            LOG_ERROR_CAT(category, __VA_ARGS__);                            \
        }                                                                    \
    } while (0)
```

**`GetMonotonicSeconds` helper** used by throttle macros:

```cpp
/// @file core/diagnostics/Logger.h (addition)

namespace Diagnostics {

/// Monotonic clock in seconds. Used by throttle macros.
/// Wraps std::chrono::steady_clock to avoid #include <chrono> in macro headers.
double GetMonotonicSeconds();

} // namespace Diagnostics
```

**Usage examples**:

```cpp
// Warn once about a deprecated code path — never floods the log
LOG_WARN_ONCE_CAT(LogCategory::Renderer,
    "Legacy forward pass is deprecated; migrate to deferred");

// Texture fallback happens every frame — throttle to once per 5 seconds
LOG_WARN_THROTTLE_CAT(LogCategory::Graphics, 5.0,
    "Texture '{}' not found, using fallback", texturePath);

// Conditional: only log when a specific debug flag is set
LOG_WARN_COND_CAT(m_DebugShadows, LogCategory::Renderer,
    "Shadow cascade {} has zero coverage", cascadeIndex);
```

### 3.5 Sinks

```cpp
void Logger::Init(const std::filesystem::path &logFilePath)
{
    // See Section 3.6 for the full async version of Init().
    // This simplified version shows the sink setup and pattern configuration.

    // Sink 1: Colored console (same as current behavior)
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%T] [%n] [%^%l%$] %v");

    // Sink 2: Rotating file — survives crashes, limited disk usage
    //   5 MB per file, 3 rotated files = max 15 MB on disk
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFilePath.string(), 5 * 1024 * 1024, 3);

    // File sink uses custom formatter with frame number (see 3.7)
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<FrameFlag>('@', "frame");
    formatter->set_pattern("[%Y-%m-%d %T.%e] [%t] [F%@frame] [%n] [%l] %v");
    fileSink->set_formatter(std::move(formatter));

    s_Sinks = { consoleSink, fileSink };
    s_GlobalLevel = spdlog::level::trace;

    // Pre-register default categories (as async loggers — see 3.6)
    for (const char *cat : {
        LogCategory::Core, LogCategory::Graphics, LogCategory::Renderer,
        LogCategory::Shader, LogCategory::Input, LogCategory::FileSystem,
        LogCategory::Window, LogCategory::ImGui, LogCategory::Demo })
    {
        GetLogger(cat);
    }

    spdlog::flush_every(std::chrono::seconds(3));
}
```

**Pattern format explained** (file sink):
```
[2026-03-25 14:32:01.234] [12340] [F00004823] [Shader] [error] Compilation failed (PBR.frag): ...
 ^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^   ^^^^^^^^^   ^^^^^^   ^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 full timestamp             thread   frame #    category  level   message
```

**Flush strategy**:
- Periodic flush every 3 seconds via `spdlog::flush_every`.
- Immediate flush on `LOG_ERROR` and `LOG_CRITICAL` (configured per-sink level trigger).
- Forced flush in the crash handler before termination.

### 3.6 Asynchronous Logging Mode

In a synchronous logging model, every `LOG_*` call blocks the calling thread until the
message is formatted and written to all sinks. For console output this is negligible, but
file I/O and future network sinks can stall the render thread for milliseconds — especially
on spinning disks or when the OS flushes write buffers.

**How UE5 handles this**: A dedicated log writer thread consumes from a lock-free ring buffer.
The main thread only performs a single atomic enqueue operation (~10ns).

**Our approach**: spdlog provides `spdlog::async_logger` with a thread pool, giving us the
same architecture without custom threading code.

```cpp
/// @file core/diagnostics/Logger.cpp (async mode)

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

void Logger::Init(const std::filesystem::path &logFilePath)
{
    // Create the thread pool: 8192-slot queue, 1 worker thread.
    // The queue is a lock-free MPSC ring buffer (bounded, non-blocking enqueue).
    // If the queue is full, the oldest message is dropped (overflow policy).
    spdlog::init_thread_pool(8192, 1);

    // Sink 1: Colored console
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%T] [%n] [%^%l%$] %v");

    // Sink 2: Rotating file
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFilePath.string(), 5 * 1024 * 1024, 3);
    fileSink->set_pattern("[%Y-%m-%d %T.%e] [%t] [F%@frame] [%n] [%l] %v");
    //                                       ^^^   ^^^^^^^^
    //                                    thread ID  frame # (custom flag, see 3.7)

    s_Sinks = { consoleSink, fileSink };
    s_GlobalLevel = spdlog::level::trace;

    // Pre-register default categories as async loggers
    for (const char *cat : {
        LogCategory::Core, LogCategory::Graphics, LogCategory::Renderer,
        LogCategory::Shader, LogCategory::Input, LogCategory::FileSystem,
        LogCategory::Window, LogCategory::ImGui, LogCategory::Demo })
    {
        auto logger = std::make_shared<spdlog::async_logger>(
            cat, s_Sinks.begin(), s_Sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::overrun_oldest);
        logger->set_level(s_GlobalLevel);
        spdlog::register_logger(logger);
    }

    spdlog::flush_every(std::chrono::seconds(3));
}
```

**Key design decisions**:

| Decision | Rationale |
|----------|-----------|
| Queue size = 8192 | ~64KB memory. Handles bursts of 8K messages before dropping. Sufficient for a single-app rendering engine. |
| 1 worker thread | File I/O is sequential anyway. Multiple workers add contention without benefit for our sink set. |
| `overrun_oldest` policy | Prefer losing old log messages over blocking the render thread. The alternative `block` policy would introduce frame hitches. |
| Console sink stays synchronous | Console output is already buffered by the OS terminal. Async adds latency to interactive debugging. The console sink is attached to async loggers but its internal `stdout` write is fast enough. |

**Fallback for crash safety**: The crash handler calls `spdlog::shutdown()` which
flushes the async queue and joins the worker thread, ensuring all buffered messages
reach disk before termination.

**Migration note**: The switch from sync to async is entirely internal to `Logger::Init()`.
No macro or call-site changes are needed. The only observable difference is that log
messages may appear slightly out-of-order across categories (each category enqueues
independently). Within a single category, ordering is preserved.

### 3.7 Frame Number and Thread ID Injection

Every log message in a real-time engine should carry the **frame number** at which it was
produced. This is critical for debugging rendering artifacts ("the shadow flickered on
frame 4823") and correlating logs with GPU captures (RenderDoc frame snapshots).

**How UE5 does it**: `GFrameCounter` is a global atomic counter incremented once per frame.
The log formatter reads it automatically.

**Our approach**: A custom spdlog formatter flag that reads the frame counter from `Time`.

```cpp
/// @file core/diagnostics/FrameFormatter.h

#include <spdlog/pattern_formatter.h>

namespace Diagnostics {

/// Custom spdlog flag '%@frame' that injects the current frame number.
/// Usage in pattern: "[F%@frame]" → "[F00004823]"
class FrameFlag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg &,
                const std::tm &,
                spdlog::memory_buf_t &dest) override
    {
        // Read the global frame counter (atomic, lock-free).
        uint64_t frame = Diagnostics::GetFrameNumber();
        // Zero-padded 8-digit frame number for alignment in log files.
        auto str = fmt::format("{:08}", frame);
        dest.append(str.data(), str.data() + str.size());
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return std::make_unique<FrameFlag>();
    }
};

/// The frame counter. Incremented once per frame by the main loop.
/// Exposed as an atomic for thread-safe reads from async logger threads.
void IncrementFrameNumber();
uint64_t GetFrameNumber();

} // namespace Diagnostics
```

**Integration with the main loop** (in `Application::Run`):

```cpp
while (m_Running && !m_Window->ShouldClose())
{
    Diagnostics::IncrementFrameNumber();  // ← first thing each frame
    m_Window->PollEvents();
    // ... rest of frame ...
}
```

**Registering the custom flag** (in `Logger::Init`):

```cpp
auto formatter = std::make_unique<spdlog::pattern_formatter>();
formatter->add_flag<FrameFlag>('@', "frame");  // register %@frame
formatter->set_pattern("[%Y-%m-%d %T.%e] [%t] [F%@frame] [%n] [%l] %v");
fileSink->set_formatter(std::move(formatter));
```

**Resulting log output** (file sink):

```
[2026-03-25 14:32:01.234] [12340] [F00004823] [Shader]   [error] Compilation failed ...
[2026-03-25 14:32:01.235] [12340] [F00004823] [Renderer] [warn]  Shadow cascade 2 has zero coverage
[2026-03-25 14:32:01.300] [12344] [F00004824] [Graphics] [info]  Texture loaded: albedo.png
 ^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^   ^^^^^^^^^   ^^^^^^^^^  ^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^
 full timestamp             thread  frame #     category   level   message
```

**Thread ID** (`%t`) is built into spdlog — no custom code needed. It identifies which
thread produced the message, which becomes essential when the engine adopts multi-threaded
rendering or job systems.

### 3.8 Runtime Level Control

```cpp
/// Change a single category's verbosity from the ImGui debug console or code:
Diagnostics::Logger::SetLevel(LogCategory::Shader, spdlog::level::warn);

/// Suppress all trace/debug output globally:
Diagnostics::Logger::SetGlobalLevel(spdlog::level::info);
```

The ImGui debug console (Layer 4) exposes this as a command:
```
> log.level Shader warn
> log.level * info
```

For command parsing and config-file ingestion, internal enum-backed controls such as
`DiagnosticLevel`, `SinkMode`, or `JsonLogState` should use `magic_enum::enum_cast`
to convert string tokens like `warn`, `info`, or `off` into strongly typed values.
The category itself remains a string key.

### 3.9 Compile-Time Level Stripping

In shipping builds, `Trace` and `Debug` level logs serve no purpose — they bloat the
binary with format strings, increase instruction cache pressure, and expose internal
details. UE5 strips `VeryVerbose`/`Verbose` logs entirely from Shipping builds via
preprocessor macros, so the format strings never enter the binary.

**Our approach**: A compile-time minimum level that elides low-severity macros to no-ops.

```cpp
/// @file core/diagnostics/LogMacros.h (top section)

// Define RTRLAB_LOG_MIN_LEVEL to strip log calls below this level at compile time.
// Levels: 0=Trace, 1=Debug, 2=Info, 3=Warn, 4=Error, 5=Critical, 6=Off
//
// Set via CMake: target_compile_definitions(RTRLab PRIVATE RTRLAB_LOG_MIN_LEVEL=2)
//   Debug build:   RTRLAB_LOG_MIN_LEVEL=0  (all logs compiled in)
//   Release build: RTRLAB_LOG_MIN_LEVEL=2  (trace/debug stripped)
//   Shipping:      RTRLAB_LOG_MIN_LEVEL=3  (only warn+ compiled in)

#ifndef RTRLAB_LOG_MIN_LEVEL
    #ifdef NDEBUG
        #define RTRLAB_LOG_MIN_LEVEL 2   // Release default: Info and above
    #else
        #define RTRLAB_LOG_MIN_LEVEL 0   // Debug default: everything
    #endif
#endif

// Wrap each macro with a compile-time check:
#if RTRLAB_LOG_MIN_LEVEL <= 0
    #define LOG_TRACE_CAT(category, ...)  /* existing implementation */
#else
    #define LOG_TRACE_CAT(category, ...)  ((void)0)
#endif

#if RTRLAB_LOG_MIN_LEVEL <= 2
    #define LOG_INFO_CAT(category, ...)   /* existing implementation */
#else
    #define LOG_INFO_CAT(category, ...)   ((void)0)
#endif

// WARN, ERROR, CRITICAL are never stripped — always compiled in.
```

**Impact measurement** (estimated):

| Build Config | Stripped Levels | Binary Size Reduction | Format Strings Removed |
|---|---|---|---|
| Debug | None | 0 | 0 |
| Release | Trace | ~2-5 KB | All `LOG_TRACE` strings |
| Shipping | Trace + Info | ~5-15 KB | All `LOG_TRACE` + `LOG_INFO` strings |

**CMake integration**:

```cmake
# In CMakeLists.txt
target_compile_definitions(RTRLab PRIVATE
    $<$<CONFIG:Debug>:RTRLAB_LOG_MIN_LEVEL=0>
    $<$<CONFIG:Release>:RTRLAB_LOG_MIN_LEVEL=2>
    $<$<CONFIG:RelWithDebInfo>:RTRLAB_LOG_MIN_LEVEL=1>
)
```

### 3.10 Structured Log Format

Text logs are human-readable but difficult for tools to parse. When logs need to flow
into external systems (log aggregators, CI failure analysis, automated test harnesses),
a machine-readable format is valuable.

**Our approach**: An optional JSON Lines (.jsonl) sink that can be enabled alongside
the human-readable text sink. Each log entry is a single JSON object on one line.

```cpp
/// @file core/diagnostics/JsonLineSink.h

/// spdlog sink that writes one JSON object per log message.
/// Output format (JSON Lines — one object per line, no trailing comma):
///
/// {"ts":"2026-03-25T14:32:01.234","frame":4823,"tid":12340,"cat":"Shader","lvl":"error","msg":"Compilation failed (PBR.frag)"}
/// {"ts":"2026-03-25T14:32:01.235","frame":4823,"tid":12340,"cat":"Renderer","lvl":"warn","msg":"Shadow cascade 2 has zero coverage"}
///
class JsonLineSink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg &msg) override;
    void flush_() override;

private:
    std::ofstream m_File;
};
```

**Schema**:

| Field | Type | Description |
|-------|------|-------------|
| `ts` | string (ISO 8601) | Timestamp with millisecond precision |
| `frame` | integer | Frame number at time of logging |
| `tid` | integer | Thread ID of the calling thread |
| `cat` | string | Log category name |
| `lvl` | string | Level: `trace`, `info`, `warn`, `error`, `critical` |
| `msg` | string | Formatted message text |

If the diagnostics system later adds enum-backed fields such as `severity`,
`recoverability`, `sink`, or `assert_kind`, they should serialize via
`magic_enum::enum_name()` so JSON output stays readable while the internal API
remains strongly typed.

**When to enable**: The JSON sink is **not registered by default**. Enable it via:
- Compile-time: `#define RTRLAB_ENABLE_JSON_LOG`
- Runtime: `Logger::EnableJsonSink("saved/logs/RTRLab.jsonl")`
- ImGui console: `log.json on`

**Why JSON Lines, not plain JSON**: JSON Lines (one JSON object per line, no wrapping
array) can be appended to without reading the entire file. It survives truncation
(each line is independently parseable). Tools like `jq` can process it directly:

```bash
# Find all shader errors on frame 4823
cat saved/logs/RTRLab.jsonl | jq 'select(.cat=="Shader" and .lvl=="error" and .frame==4823)'
```

---

## 4. Layer 1 — Assertion Framework

### 4.1 Assertion Macros

Three tiers, inspired by Unreal's `check`/`verify`/`ensure`:

```cpp
/// @file core/diagnostics/Assert.h

#include "core/diagnostics/LogMacros.h"

// --- RTRLAB_ASSERT(condition) ---
// Hard assertion. Fires in ALL builds (Debug AND Release).
// On failure: logs error with file/line, captures callstack, breaks into debugger
// (Debug) or terminates via crash handler (Release).
// Use for invariants that MUST hold — violation means a bug that corrupts state.
//
// Equivalent to: Unreal's check(), Godot's CRASH_COND()

#define RTRLAB_ASSERT(condition)                                              \
    do {                                                                      \
        if (!(condition)) [[unlikely]] {                                      \
            ::Diagnostics::Detail::OnAssertionFailed(                         \
                #condition, __FILE__, __LINE__, __FUNCTION__, nullptr);       \
        }                                                                     \
    } while (0)

#define RTRLAB_ASSERTF(condition, fmt, ...)                                   \
    do {                                                                      \
        if (!(condition)) [[unlikely]] {                                      \
            auto _msg = fmt::format(fmt, ##__VA_ARGS__);                     \
            ::Diagnostics::Detail::OnAssertionFailed(                         \
                #condition, __FILE__, __LINE__, __FUNCTION__, _msg.c_str());  \
        }                                                                     \
    } while (0)


// --- RTRLAB_VERIFY(expr) ---
// Like ASSERT, but the expression is ALWAYS evaluated (even if assertions were
// hypothetically compiled out). Use when the expression has side effects.
//
// Equivalent to: Unreal's verify()

#define RTRLAB_VERIFY(expr) RTRLAB_ASSERT(expr)


// --- RTRLAB_ENSURE(condition) ---
// Soft assertion. Does NOT terminate. Logs error + callstack on first occurrence,
// then continues execution. Returns false when the condition fails, so callers
// can handle the failure gracefully.
// Use for "this shouldn't happen but the engine can survive if it does".
//
// Equivalent to: Unreal's ensure()

#define RTRLAB_ENSURE(condition)                                              \
    (  [&]() [[unlikely]] {                                                   \
        if (!(condition)) {                                                   \
            static bool _reported = false;                                    \
            if (!_reported) {                                                 \
                _reported = true;                                             \
                ::Diagnostics::Detail::OnEnsureFailed(                        \
                    #condition, __FILE__, __LINE__, __FUNCTION__, nullptr);   \
            }                                                                 \
            return false;                                                     \
        }                                                                     \
        return true;                                                          \
    }() )

#define RTRLAB_ENSUREF(condition, fmt, ...)                                   \
    (  [&]() [[unlikely]] {                                                   \
        if (!(condition)) {                                                   \
            static bool _reported = false;                                    \
            if (!_reported) {                                                 \
                _reported = true;                                             \
                auto _msg = fmt::format(fmt, ##__VA_ARGS__);                 \
                ::Diagnostics::Detail::OnEnsureFailed(                        \
                    #condition, __FILE__, __LINE__, __FUNCTION__,             \
                    _msg.c_str());                                            \
            }                                                                 \
            return false;                                                     \
        }                                                                     \
        return true;                                                          \
    }() )
```

### 4.2 Assertion Handler

```cpp
/// @file core/diagnostics/Assert.cpp

namespace Diagnostics::Detail {

void OnAssertionFailed(const char *expr, const char *file, int line,
                       const char *func, const char *msg)
{
    // 1. Log the assertion failure at CRITICAL level
    if (msg)
        LOG_CRITICAL_CAT("Assert",
            "Assertion failed: {} ({})\n  in {} ({}:{})", expr, msg, func, file, line);
    else
        LOG_CRITICAL_CAT("Assert",
            "Assertion failed: {}\n  in {} ({}:{})", expr, func, file, line);

    // 2. Capture and log callstack (platform-specific, see Section 6)
    std::string callstack = CaptureCallstack(/*framesToSkip=*/2);
    LOG_CRITICAL_CAT("Assert", "Callstack:\n{}", callstack);

    // 3. Flush all logs immediately
    Logger::Flush();

    // 4. Break into an attached debugger on any supported platform, then terminate.
    BreakInDebuggerIfAttached();
    std::abort();
}

void OnEnsureFailed(const char *expr, const char *file, int line,
                    const char *func, const char *msg)
{
    // Same logging as OnAssertionFailed, but NO termination.
    if (msg)
        LOG_ERROR_CAT("Ensure",
            "Ensure failed: {} ({})\n  in {} ({}:{})", expr, msg, func, file, line);
    else
        LOG_ERROR_CAT("Ensure",
            "Ensure failed: {}\n  in {} ({}:{})", expr, func, file, line);

    std::string callstack = CaptureCallstack(/*framesToSkip=*/2);
    LOG_ERROR_CAT("Ensure", "Callstack:\n{}", callstack);

    // No break, no terminate — caller handles the failure.
}

} // namespace Diagnostics::Detail
```

### 4.3 Replacing Existing assert() Usage

Current `assert()` call sites map to the new macros as follows:

| Current Usage | Replacement | Rationale |
|---------------|-------------|-----------|
| `assert(m_Framebuffer)` in render passes | `RTRLAB_ASSERT(m_Framebuffer)` | Null framebuffer = certain crash; hard assert |
| `assert(index < m_ColorAttachments.size())` | `RTRLAB_ASSERT(index < m_ColorAttachments.size())` | Out-of-bounds = memory corruption; hard assert |
| `assert(layout.GetStride() > 0)` | `RTRLAB_ASSERT(layout.GetStride() > 0)` | Zero stride = broken rendering; hard assert |
| `assert(!GetDevice())` singleton checks | `RTRLAB_ASSERT(!GetDevice())` | Double init = logic error; hard assert |
| Precondition checks in Mesh data builders | `RTRLAB_ASSERT(!positions.empty())` | Empty mesh = caller bug; hard assert |

**Rule of thumb**: If the current code uses `assert()`, replace with `RTRLAB_ASSERT`.
If code currently silently ignores a condition, evaluate whether `RTRLAB_ENSURE` is appropriate.

---

## 5. Layer 2 — Error Handling Policy

### 5.1 Error Classification

Every error in the engine falls into one of four severity tiers:

Internally, this layer may represent policy as enum classes:

```cpp
enum class DiagnosticSeverity { Trace, Info, Warning, Error, Critical };
enum class DiagnosticRecoverability { Recoverable, Fatal };
enum class AssertionKind { Assert, Verify, Ensure };
```

These enums are a good `magic_enum` fit:
- `enum_name()` for structured logs, crash summaries, and test failure output
- `enum_cast()` for config-driven policy, debug-console commands, and scripted tooling
- `enum_entries()` for UI dropdowns or help text

The log category itself still remains a string, not an enum, to preserve extensibility.

```
┌─────────────────────────────────────────────────────────────────────┐
│ Severity       │ Response           │ Macro / Pattern              │
├────────────────┼────────────────────┼──────────────────────────────┤
│ Warning        │ Log, continue      │ LOG_WARN_CAT(...)            │
│                │                    │                              │
│ Recoverable    │ Log, early return  │ ERR_FAIL_COND_V(cond, ret)   │
│ Error          │ (skip operation)   │ ERR_FAIL_COND(cond)          │
│                │                    │                              │
│ Soft Assert    │ Log + callstack,   │ RTRLAB_ENSURE(cond)          │
│ (shouldn't     │ continue           │                              │
│  happen)       │                    │                              │
│                │                    │                              │
│ Fatal          │ Log + callstack,   │ RTRLAB_ASSERT(cond)          │
│                │ terminate          │ RTRLAB_ASSERTF(cond, ...)    │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 Error Return Macros

Inspired by Godot's `ERR_FAIL_*` family. These replace the current `LOG_ERROR` + `throw`
pattern for recoverable errors.

```cpp
/// @file core/diagnostics/ErrorMacros.h

// Preferred in engine code when a subsystem-specific category matters:
#define ERR_FAIL_COND_CAT(category, condition)                                \
    do {                                                                      \
        if (condition) [[unlikely]] {                                         \
            LOG_ERROR_CAT(category,                                           \
                "{}:{}: condition '{}' failed",                               \
                __FILE__, __LINE__, #condition);                              \
            return;                                                           \
        }                                                                     \
    } while (0)

#define ERR_FAIL_COND_V_CAT(category, condition, retval)                      \
    do {                                                                      \
        if (condition) [[unlikely]] {                                         \
            LOG_ERROR_CAT(category,                                           \
                "{}:{}: condition '{}' failed",                               \
                __FILE__, __LINE__, #condition);                              \
            return (retval);                                                  \
        }                                                                     \
    } while (0)

#define ERR_FAIL_COND_MSG_CAT(category, condition, message)                   \
    do {                                                                      \
        if (condition) [[unlikely]] {                                         \
            LOG_ERROR_CAT(category,                                           \
                "{}:{}: condition '{}' failed: {}",                           \
                __FILE__, __LINE__, #condition, message);                     \
            return;                                                           \
        }                                                                     \
    } while (0)

#define ERR_FAIL_COND_V_MSG_CAT(category, condition, retval, message)         \
    do {                                                                      \
        if (condition) [[unlikely]] {                                         \
            LOG_ERROR_CAT(category,                                           \
                "{}:{}: condition '{}' failed: {}",                           \
                __FILE__, __LINE__, #condition, message);                     \
            return (retval);                                                  \
        }                                                                     \
    } while (0)

// Convenience aliases when the generic "Error" category is sufficient:
#define ERR_FAIL_COND(condition) \
    ERR_FAIL_COND_CAT(LogCategory::Error, condition)

#define ERR_FAIL_COND_V(condition, retval) \
    ERR_FAIL_COND_V_CAT(LogCategory::Error, condition, retval)

#define ERR_FAIL_COND_MSG(condition, message) \
    ERR_FAIL_COND_MSG_CAT(LogCategory::Error, condition, message)

#define ERR_FAIL_COND_V_MSG(condition, retval, message) \
    ERR_FAIL_COND_V_MSG_CAT(LogCategory::Error, condition, retval, message)
```

Category-aware `_CAT` variants keep recoverable failures aligned with the subsystem's logger
(`FileSystem`, `Graphics`, `Shader`, etc.), while the non-category aliases remain available for
generic utility code and quick migration.

### 5.3 Migration from Exceptions

Current exception sites and their replacements:

| File | Current Code | Replacement |
|------|-------------|-------------|
| `Application.cpp:29` | `throw std::runtime_error("Application already exists.")` | `RTRLAB_ASSERT(!s_Instance)` — double-init is a fatal logic error |
| `Window.cpp:46` | `throw std::runtime_error("Failed to initialize GLFW.")` | `RTRLAB_ASSERT(glfwInit())` — no GLFW = cannot continue |
| `Window.cpp:69` | `throw std::runtime_error("Failed to create GLFW window.")` | `RTRLAB_ASSERT(m_Handle)` — no window = cannot continue |
| `Window.cpp:77` | `throw std::runtime_error("Failed to initialize GLAD.")` | `RTRLAB_ASSERT(gladLoadGLLoader(...))` — no GL = cannot continue |
| `FileSystem.cpp:157` | `throw std::runtime_error("Failed to open file: ...")` | Return `std::optional<std::string>{}` + `LOG_ERROR_CAT` — caller checks |
| `FileSystem.cpp:168` | `throw std::runtime_error("Failed to open binary file: ...")` | Return `std::optional<std::vector<uint8_t>>{}` + `LOG_ERROR_CAT` — caller checks |
| `GLShader.cpp:79` | `throw std::runtime_error("Shader compilation failed...")` | Return `false` from compile function + `LOG_ERROR_CAT(LogCategory::Shader, ...)` |
| `GLShader.cpp:111` | `throw std::runtime_error("Shader program link failed...")` | Return `false` from link function + `LOG_ERROR_CAT(LogCategory::Shader, ...)` |
| `GLTexture2D.cpp:177` | `throw std::runtime_error("Failed to load texture...")` | Return null `Ref<>` + `LOG_ERROR_CAT(LogCategory::Graphics, ...)` |
| `GLFramebuffer.cpp:218` | `throw std::runtime_error("Framebuffer is not complete...")` | `RTRLAB_ASSERT(status == GL_FRAMEBUFFER_COMPLETE)` — incomplete FB is fatal |
| `PropertyTree.cpp:90` | `throw std::out_of_range("PropertyTree: key not found...")` | `RTRLAB_ASSERTF(...)` — missing required serialized field is a logic/contract error |
| `InputAction.cpp:182` | `catch (const json::parse_error &e)` | Keep as-is — JSON parsing is a boundary where exceptions are acceptable (see 5.4) |
| `DemoRegistry.cpp:47` | `throw std::runtime_error("Unknown demo...")` | `ERR_FAIL_COND_V_MSG(...)` returning `nullptr` — unknown demo is recoverable |

Backend-specific resource loaders follow the same contract. `GLShader` and `MetalShader` return null
`Ref<>` values on source/read/compile failure, while `GLTexture2D` and `MetalTexture2D` log and return
null objects for load/create failures instead of unwinding through exceptions.

### 5.4 Where Exceptions Remain

Exceptions are acceptable at **system boundaries** — places where third-party libraries
throw and we cannot control the API:

1. **nlohmann/json parsing** — `json::parse()` throws `json::parse_error`. Catch at the
   boundary in `JsonBackend::ReadFromString`, convert to `bool` return.
2. **std::filesystem** — Prefer `std::error_code` overloads and explicit status returns.
   If a platform-specific filesystem API still throws, catch at the boundary and convert.
3. **Third-party library constructors** — If vendor code throws, catch at the integration
   boundary and convert.

**Rule**: Never throw from engine code. Catch third-party exceptions at the boundary
and convert to the engine's error-handling patterns.

---

## 6. Layer 3 — Crash Handler

### 6.1 Windows SEH Handler

```cpp
/// @file core/diagnostics/CrashHandler.h

namespace Diagnostics {

class CrashHandler
{
public:
    /// Register platform crash handlers. Call once during Application startup,
    /// after Logger::Init().
    static void Init();

    /// Trigger a controlled fatal error — flushes logs, writes minidump, terminates.
    /// Called by RTRLAB_ASSERT on failure in Release builds.
    [[noreturn]] static void FatalError(const char *reason);

private:
    /// Windows: registered via SetUnhandledExceptionFilter
    static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS *exceptionInfo);

    /// Path to the crash dump directory (saved/logs/crashes/)
    static std::filesystem::path s_CrashDir;
};

} // namespace Diagnostics
```

### 6.2 Cross-Platform Crash Handler Abstraction

Because the engine already targets multiple platforms (Windows plus macOS today, and
Linux is an expected follow-on), diagnostics parity cannot be treated as an optional
postscript. Crash handling and callstack capture need a platform abstraction now,
following the same pattern as UE5's `FPlatformMisc` and Godot's per-platform
`crash_handler_*.cpp`.

```cpp
/// @file core/diagnostics/CrashHandler.h (extended)

namespace Diagnostics {

class CrashHandler
{
public:
    /// Register platform-specific crash handlers.
    /// - Windows: SetUnhandledExceptionFilter (SEH)
    /// - Linux:   sigaction for SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL
    /// - macOS:   sigaction (same signals) + optional Mach exception handler
    static void Init();

    /// Trigger a controlled fatal error.
    [[noreturn]] static void FatalError(const char *reason);

private:
#if defined(_WIN32)
    static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS *exceptionInfo);
#else
    static void PosixSignalHandler(int signal, siginfo_t *info, void *context);
#endif

    static std::filesystem::path s_CrashDir;
};

} // namespace Diagnostics
```

**POSIX implementation** (Linux / macOS):

```cpp
/// @file core/diagnostics/CrashHandler_Posix.cpp

#include <signal.h>
#include <execinfo.h>   // backtrace(), backtrace_symbols()
#include <cxxabi.h>      // abi::__cxa_demangle()

void CrashHandler::Init()
{
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;  // Reset to default after first catch
    sa.sa_sigaction = PosixSignalHandler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);  // Segmentation fault
    sigaction(SIGABRT, &sa, nullptr);  // abort()
    sigaction(SIGFPE,  &sa, nullptr);  // Floating-point exception
    sigaction(SIGBUS,  &sa, nullptr);  // Bus error
    sigaction(SIGILL,  &sa, nullptr);  // Illegal instruction
}

void CrashHandler::PosixSignalHandler(int signal, siginfo_t *info, void * /*context*/)
{
    // 1. Log the signal
    LOG_CRITICAL_CAT("Crash", "Received signal {} ({}) at address {}",
        signal, strsignal(signal), info->si_addr);

    // 2. Capture callstack via backtrace()
    void *frames[64];
    int count = backtrace(frames, 64);
    char **symbols = backtrace_symbols(frames, count);

    std::string callstack;
    for (int i = 2; i < count; ++i)  // skip signal handler frames
    {
        // Attempt demangling for readable C++ names
        callstack += "  ";
        callstack += symbols[i];
        callstack += "\n";
    }
    free(symbols);

    LOG_CRITICAL_CAT("Crash", "Callstack:\n{}", callstack);

    // 3. Flush and terminate
    Logger::Flush();
    _exit(1);  // _exit, not exit — avoid running atexit handlers in corrupted state
}
```

**Callstack abstraction** updated for both platforms:

```cpp
/// @file core/diagnostics/Callstack.h (updated)

namespace Diagnostics {

/// Platform-independent callstack capture.
/// - Windows: CaptureStackBackTrace (baseline) with future DbgHelp symbolization
/// - Linux/macOS: backtrace() + dladdr() + abi::__cxa_demangle()
std::string CaptureCallstack(int framesToSkip = 0, int maxFrames = 32);

} // namespace Diagnostics
```

**File layout for platform-specific code**:

```
src/core/diagnostics/
    CrashHandler.h               — Platform-independent interface
    CrashHandler_Win32.cpp       — Windows SEH + MiniDumpWriteDump
    CrashHandler_Posix.cpp       — Linux/macOS signal handler
    Callstack.h                  — Platform-independent interface
    Debugger.h / .cpp            — Debugger detection + break helper
    Callstack_Win32.cpp          — CaptureStackBackTrace-based implementation
    Callstack_Posix.cpp          — backtrace() + dladdr() + demangling implementation
```

**CMake selection**:

```cmake
if(WIN32)
    target_sources(RTRLab PRIVATE
        src/core/diagnostics/CrashHandler_Win32.cpp
        src/core/diagnostics/Callstack_Win32.cpp)
else()
    target_sources(RTRLab PRIVATE
        src/core/diagnostics/CrashHandler_Posix.cpp
        src/core/diagnostics/Callstack_Posix.cpp)
    if(UNIX AND NOT APPLE)
        target_link_libraries(RTRLab PRIVATE dl)
    endif()
endif()
```

### 6.3 Crash Sequence

When a fatal error occurs (unhandled SEH exception, `RTRLAB_ASSERT` failure, or
`CrashHandler::FatalError` call), the following sequence executes:

```
1. LOG_CRITICAL("Fatal error: <reason>")
2. Capture callstack → LOG_CRITICAL (full symbolic callstack)
3. Logger::Flush()                         ← force all buffered logs to disk
4. Write minidump to saved/logs/crashes/         ← .dmp file with exception context
5. Write crash summary to saved/logs/crashes/    ← human-readable .txt with last N log lines
6. std::terminate() / ExitProcess(1)
```

**Callstack capture** uses `<DbgHelp.h>` on Windows:

```cpp
/// @file core/diagnostics/Callstack.h

namespace Diagnostics {

/// Capture the current callstack as a human-readable string.
/// @param framesToSkip  Skip this many frames from the top (to hide diagnostics internals).
/// @param maxFrames     Maximum depth to capture.
/// @return Multi-line string: "  ModuleName!FunctionName + 0xOffset (File:Line)\n..."
std::string CaptureCallstack(int framesToSkip = 0, int maxFrames = 32);

} // namespace Diagnostics
```

Implementation uses `CaptureStackBackTrace` + `SymFromAddr` + `SymGetLineFromAddr64`
from `<DbgHelp.h>`. Symbol resolution requires `.pdb` files — Release builds should
generate PDBs (`/Zi` + `/DEBUG` linker flag).

### 6.4 Minidump Generation

```cpp
/// Write a Windows minidump (.dmp) file.
/// Requires linking against dbghelp.lib.
static bool WriteMiniDump(EXCEPTION_POINTERS *exInfo, const std::filesystem::path &path)
{
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId          = GetCurrentThreadId();
    mdei.ExceptionPointers = exInfo;
    mdei.ClientPointers    = FALSE;

    // MiniDumpNormal is small (~50KB). For richer debugging:
    // MiniDumpWithDataSegs | MiniDumpWithHandleData (~1-5MB)
    BOOL ok = MiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(), hFile,
        MiniDumpNormal, exInfo ? &mdei : nullptr,
        nullptr, nullptr);

    CloseHandle(hFile);
    return ok != FALSE;
}
```

**Dump naming convention**: `saved/logs/crashes/RTRLab_20260325_143201.dmp`
(ISO date + time, no colons for filesystem safety).

---

## 7. Layer 4 — Debug Console Integration

An in-engine ImGui console window that displays logs in real time and accepts commands
for runtime diagnostics control.

```cpp
/// @file core/diagnostics/ImGuiConsoleSink.h

/// Custom spdlog sink that pushes log messages into a ring buffer
/// for display in the ImGui debug console.
class ImGuiConsoleSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    struct LogEntry
    {
        spdlog::level::level_enum Level;
        std::string Category;
        std::string Message;
        std::string Timestamp;
    };

    /// Retrieve the ring buffer for rendering. Thread-safe snapshot.
    std::vector<LogEntry> GetEntries() const;

    /// Clear the ring buffer.
    void Clear();

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override;
    void flush_() override {}

private:
    static constexpr size_t kMaxEntries = 1024;
    std::deque<LogEntry> m_Entries;
};
```

**Console commands**:

| Command | Effect |
|---------|--------|
| `log.level <category> <level>` | Set verbosity for a category (`trace`/`info`/`warn`/`error`/`off`) |
| `log.level * <level>` | Set global verbosity floor |
| `log.filter <category>` | Show only messages from this category |
| `log.filter *` | Show all categories |
| `log.clear` | Clear the console buffer |
| `log.flush` | Force flush all sinks to disk |

For commands whose arguments map cleanly to internal enum classes—such as log level,
JSON sink state, assertion display mode, or future diagnostics policy toggles—the parser
should use `magic_enum` instead of hand-written string tables. This keeps command parsing,
config-file parsing, and UI dropdown choices consistent with the engine's typed policy enums.

**Color scheme** (matches spdlog console colors):

| Level | Color |
|-------|-------|
| Trace | Gray |
| Info | White |
| Warn | Yellow |
| Error | Red |
| Critical | Bold red / magenta background |

---

## 8. File Layout

```
src/
  core/
    diagnostics/
      Logger.h / .cpp              — Layer 0: multi-sink async spdlog wrapper
      LogCategories.h              — Layer 0: predefined category names
      LogMacros.h                  — Layer 0: LOG_*_CAT, _ONCE, _THROTTLE, _COND + legacy
      FrameFormatter.h / .cpp      — Layer 0: custom %@frame spdlog flag
      JsonLineSink.h / .cpp        — Layer 0: optional structured JSON Lines sink
      Assert.h / .cpp              — Layer 1: RTRLAB_ASSERT / VERIFY / ENSURE
      ErrorMacros.h                — Layer 2: ERR_FAIL_COND_* family
      CrashHandler.h               — Layer 3: platform-independent crash handler interface
      CrashHandler_Win32.cpp       — Layer 3: Windows SEH + MiniDumpWriteDump
      CrashHandler_Posix.cpp       — Layer 3: Linux/macOS signal handler
      Callstack.h                  — Layer 3: platform-independent callstack interface
      Debugger.h / .cpp            — Layer 1: cross-platform debugger detection / trap helper
      Callstack_Win32.cpp          — Layer 3: CaptureStackBackTrace-based callstack capture
      Callstack_Posix.cpp          — Layer 3: backtrace() + dladdr() + demangling capture
      ImGuiConsoleSink.h / .cpp    — Layer 4: ring buffer sink for ImGui
    Logger.h                       — DEPRECATED: thin redirect to diagnostics/LogMacros.h
                                     (removed after full migration)
saved/
  logs/
    RTRLab.log                     — Rotating log file (runtime output, human-readable)
    RTRLab.1.log                   — Previous rotation
    RTRLab.2.log                   — Oldest rotation
    RTRLab.jsonl                   — Structured JSON Lines log (optional, when enabled)
    crashes/
      RTRLab_YYYYMMDD_HHMMSS.dmp  — Minidump files (Windows only)
      RTRLab_YYYYMMDD_HHMMSS.txt  — Human-readable crash summary
```

The `diagnostics/` module lives under `core/` because it is engine infrastructure
depended on by every subsystem. The runtime log path is resolved through the engine's
saved-data policy, so Debug builds write under the source tree's `saved/logs/` while
Release builds write under the platform user-data directory (for example
`%LOCALAPPDATA%/RTRLab/logs/` on Windows). Platform-specific source files
are conditionally compiled via CMake (see Section 6.2).

---

## 9. Migration Plan

### Phase 1 — Logger upgrade (multi-sink + categories)

**Goal**: Add file logging and category support. Zero behavior change for existing code —
old `LOG_*` macros continue to work, routing to the "Core" category.

| Step | Deliverable | Test |
|------|-------------|------|
| 1a | `Diagnostics::Logger` with console + rotating file sinks | Manual: verify `saved/logs/RTRLab.log` is created and written |
| 1b | `LogCategories.h` with predefined category names | Compile test: all categories resolve |
| 1c | `LogMacros.h` with `LOG_*_CAT` + legacy `LOG_*` compatibility | Existing code compiles unchanged; logs show `[Core]` prefix |
| 1d | Update `Application.cpp` to call `Diagnostics::Logger::Init(FileSystem::GetSavedPath("logs/RTRLab.log"))` | Application starts normally with new logger |
| 1e | Migrate Window.cpp to `LOG_*_CAT(LogCategory::Window, ...)` | Window logs show `[Window]` category |
| 1f | Migrate GLShader.cpp to `LOG_*_CAT(LogCategory::Shader, ...)` | Shader logs show `[Shader]` category |

**Backward compatibility**: The old `Logger` class and `Logger.h` become a thin
redirect that includes `diagnostics/LogMacros.h`. Remove after all call sites are
migrated.

### Phase 2 — Assertion framework

**Goal**: Replace all `assert()` with `RTRLAB_ASSERT`, add `RTRLAB_ENSURE` for
soft assertions, and make assertion diagnostics meaningful on every supported platform.
Assertions are active in all build configurations.

| Step | Deliverable | Test |
|------|-------------|------|
| 2a | `Assert.h` with `RTRLAB_ASSERT` / `RTRLAB_VERIFY` / `RTRLAB_ENSURE` | Unit test: ensure failure returns false without terminating |
| 2b | `Callstack_Win32.cpp` + `Callstack_Posix.cpp` with `CaptureCallstack` | Manual: verify callstack output on Windows and macOS/Linux |
| 2c | `Debugger.h/.cpp` with cross-platform debugger detection and trap helper | Manual: debugger-attached assert breaks on each supported platform |
| 2d | Replace `assert()` in `GLFramebuffer.cpp`, `GLVertexArray.cpp` | Release build still checks these conditions |
| 2e | Replace `assert()` in `SceneRenderer.cpp`, `Mesh.cpp` | Release build still checks |
| 2f | Replace `assert()` in `GraphicsDevice.cpp` | Release build still checks |
| 2g | Remove `#include <cassert>` from all engine source files | Grep confirms no remaining `assert(` in src/ |

### Phase 3 — Error handling policy

**Goal**: Replace `throw std::runtime_error` with appropriate error macros or
assert macros. Eliminate exceptions from engine code.

| Step | Deliverable | Test |
|------|-------------|------|
| 3a | `ErrorMacros.h` with `ERR_FAIL_COND_*` family | Unit test: macro logs and returns |
| 3b | Migrate `Application.cpp` — replace throws with `RTRLAB_ASSERT` | Fatal path still terminates cleanly |
| 3c | Migrate `Window.cpp` — replace throws with `RTRLAB_ASSERT` | Init failures still detected |
| 3d | Migrate `FileSystem.cpp` — change return types to `std::optional` | Callers updated to check optional |
| 3e | Migrate shader backends (`GLShader.cpp`, `MetalShader.mm`) — compile/load failures return null objects | Shader errors logged, no throw |
| 3f | Migrate `GLTexture2D.cpp` — load returns null `Ref<>` on failure | Texture errors logged, no throw |
| 3g | Migrate `DemoRegistry.cpp` — use `ERR_FAIL_COND_V_MSG` | Unknown demo returns nullptr gracefully |
| 3h | Audit all `catch` blocks — keep only third-party boundary catches | No engine code throws |

Phase 3 is complete when the only remaining `catch` sites are deliberate third-party boundaries,
such as `nlohmann::json` parsing and optional reflection-sidecar parsing. Engine-owned control flow,
resource creation, filesystem reads, and recoverable runtime errors should no longer rely on exceptions.

### Phase 4 — Crash handler

**Goal**: Register platform-appropriate crash handlers that preserve diagnostics on
every supported platform: Windows SEH on Win32, POSIX signal handling on Linux/macOS.

| Step | Deliverable | Test |
|------|-------------|------|
| 4a | `CrashHandler_Win32.cpp` and `CrashHandler_Posix.cpp` behind one interface | Manual: deliberate fault produces persisted diagnostics on each supported platform |
| 4b | `WriteMiniDump` on Windows using DbgHelp | Verify `.dmp` opens in Visual Studio / WinDbg |
| 4c | POSIX signal-path crash summary writer | Verify Linux/macOS produce readable `.txt` crash output |
| 4d | Hook into `Application` startup: `CrashHandler::Init()` after `Logger::Init()` | Init order verified |
| 4e | CMake: conditional source selection and platform link settings (`dbghelp`/`dl`) | All supported platforms compile and link correctly |
| 4f | Windows Release symbol settings (`/Zi` + `/DEBUG`) | PDB files generated in Release |

### Phase 5 — Debug console integration

**Goal**: ImGui console window with live log display and runtime commands.

| Step | Deliverable | Test |
|------|-------------|------|
| 5a | `ImGuiConsoleSink` ring buffer sink | Logs appear in ImGui window |
| 5b | Register sink in `Logger::Init()` | All categories visible in console |
| 5c | Console command parser (`log.level`, `log.filter`, etc.) | Commands change behavior at runtime |
| 5d | Category filter dropdown + level selector | UI controls work |
| 5e | Color-coded log entries by severity | Visual verification |

### Phase 6 — Extended logging features

**Goal**: Close the remaining gaps with production game engines — frame context,
log flood prevention, async I/O, compile-time stripping, and structured output.

| Step | Deliverable | Test |
|------|-------------|------|
| 6a | `FrameFormatter` custom spdlog flag (`%@frame`) | Log file contains `[F00000001]` on first frame |
| 6b | `IncrementFrameNumber()` call in `Application::Run` | Frame counter increments each frame |
| 6c | Thread ID in file sink pattern (`%t`) | Multi-threaded log entries show distinct thread IDs |
| 6d | `LOG_WARN_ONCE_CAT` / `LOG_ERROR_ONCE_CAT` macros | Second call at same site produces no output |
| 6e | `LOG_WARN_THROTTLE_CAT` / `LOG_ERROR_THROTTLE_CAT` macros | Messages suppressed within interval, emitted after interval |
| 6f | `LOG_WARN_COND_CAT` / `LOG_ERROR_COND_CAT` macros | Conditional logging only when condition is true |
| 6g | `GetMonotonicSeconds()` helper for throttle macros | Returns monotonic time independent of system clock changes |
| 6h | Switch `Logger::Init` to async mode (`spdlog::async_logger`) | Verify log output unchanged; measure main-thread overhead reduction |
| 6i | Compile-time level stripping via `RTRLAB_LOG_MIN_LEVEL` | Release build: `LOG_TRACE` calls produce no code (inspect assembly or binary size) |
| 6j | CMake: per-config `RTRLAB_LOG_MIN_LEVEL` definitions | Debug=0, RelWithDebInfo=1, Release=2 |
| 6k | `JsonLineSink` structured log sink | Verify `.jsonl` output is valid JSON Lines; parseable by `jq` |
| 6l | `Logger::EnableJsonSink()` runtime toggle | JSON sink activates/deactivates without restart |

**Priority within Phase 6** (recommended implementation order):

```
High impact, low effort:
  6a-6c  Frame number + thread ID injection     (~30 lines, immediate debugging value)
  6d-6g  ONCE / THROTTLE / COND macros          (~60 lines, prevents log floods)

Medium impact, low effort:
  6i-6j  Compile-time level stripping            (~20 lines of macros + CMake)

Medium impact, medium effort:
  6h     Async logging mode                      (~1 line change in Init, but needs testing)
  6k-6l  JSON Lines sink                         (~80 lines, useful for tooling)

Cross-platform diagnostics parity is no longer listed here because it is baseline work,
not an optional extension for hypothetical future ports.
```

---

## 10. Appendix A: Industry Survey

### A1. Unreal Engine 5

**Logging**: `UE_LOG(LogCategory, Verbosity, Format, ...)` macro system.
Categories are declared with `DECLARE_LOG_CATEGORY_EXTERN` / `DEFINE_LOG_CATEGORY`.
Output goes to console, `Saved/Logs/<Project>.log`, and the Editor's Message Log panel.
Per-category verbosity is controllable at runtime via console commands (`Log <Category> <Level>`).

**Assertions**:
- `check(expr)` — fatal in all builds, triggers crash reporter.
- `checkf(expr, fmt, ...)` — fatal with formatted message.
- `verify(expr)` — same as `check` but expression always evaluated.
- `ensure(expr)` — non-fatal, logs callstack + sends to crash reporter, continues execution. Reports only once per call site.
- `ensureMsgf(expr, fmt, ...)` — `ensure` with message.

**Error handling**: No C++ exceptions (`-fno-exceptions`). Errors use `check`/`ensure` + error return values. Subsystems use `EError`-style enums.

**Crash handling**: `FPlatformMisc::SetCrashHandler` registers platform-specific handlers. On crash: captures callstack, writes minidump, launches `CrashReportClient` process that can upload to Epic's backend. SEH on Windows, signal handlers on Linux/Mac.

### A2. Godot Engine

**Logging**: `print_line`, `print_verbose`, `WARN_PRINT`, `ERR_PRINT` macros with file/line. Output to `user://logs/godot.log` with automatic rotation.

**Assertions / Error handling**:
- `ERR_FAIL_COND(cond)` — log error, return void.
- `ERR_FAIL_COND_V(cond, retval)` — log error, return value.
- `ERR_FAIL_COND_MSG(cond, msg)` — with custom message.
- `ERR_FAIL_INDEX(idx, size)` — bounds check variant.
- `CRASH_COND(cond)` — truly fatal (abort).
- `DEV_ASSERT(cond)` — debug-only.

Philosophy: **almost everything is recoverable**. `ERR_FAIL_*` is used hundreds of times across the codebase. Only `CRASH_COND` terminates. No C++ exceptions.

**Crash handling**: Signal-based handler on Unix, SEH on Windows. Writes error log with callstack.

### A3. CryEngine / O3DE

**Logging**: `CryLog`, `CryWarning`, `CryFatalError` with verbosity levels and subsystem IDs.

**Assertions**: `CRY_ASSERT(cond)` — always active, breaks into debugger, generates crash report in Release.

**Error handling**: No exceptions. `CryFatalError` terminates with diagnostics. Recoverable errors return status codes.

---

## 11. Appendix B: Alternatives Considered

### B1. Keep std::runtime_error everywhere

**Pros**: Already implemented. Familiar C++ pattern.

**Cons**: Exception overhead in performance-sensitive code paths. Stack unwinding cost
is non-zero even when not throwing (binary size for unwind tables, missed optimizations).
More importantly, catch sites are inconsistent — some callers catch, some don't, leading
to unpredictable termination points. No mainstream game engine uses exceptions internally.

**Verdict**: Acceptable at third-party boundaries. Not suitable as the primary error
mechanism for engine code.

### B2. Full Result<T, E> monadic error type

A Rust-style `Result<T, Error>` with `map`, `and_then`, `or_else`:

```cpp
template<typename T, typename E = std::string>
class Result { ... };

Result<Ref<Texture2D>> LoadTexture(const std::string &path);
```

**Pros**: Type-safe, composable, forces callers to handle errors.

**Cons**: Significant API churn — every function returning a value needs to change its
signature. Verbose without C++ pattern matching. Overhead of `std::variant` in hot paths.
Overkill for a project where most errors are "log and skip" or "log and abort".

**Verdict**: Excellent for library APIs and system boundaries. Too heavyweight as a
universal pattern in a rendering engine. The `ERR_FAIL_COND_V` + `RTRLAB_ASSERT`
approach achieves 90% of the safety with 10% of the API surface change.

### B3. Third-party crash reporter (Breakpad / Crashpad)

Google's Breakpad/Crashpad or Sentry's native SDK for crash collection and symbolication.

**Pros**: Battle-tested, cross-platform, handles edge cases (corrupted stacks, out-of-memory
crashes), includes symbol server integration.

**Cons**: Heavy dependency (~50K+ LOC). Requires running a symbol server. Build system
integration is non-trivial. Overkill for a learning/research project without a user base
generating crash reports.

**Verdict**: Right choice for a shipped game with millions of users. For this project,
a simple `MiniDumpWriteDump` + `CaptureStackBackTrace` covers the diagnostic needs
without the complexity. Can be added later if the project grows.
