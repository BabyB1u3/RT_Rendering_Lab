# Input & Event System Code Review

Code review of the `src/Core/Input/`, `src/Core/Event/`, and associated test code, based
on the implementation as of 2026-04-09.

> **Overall assessment**: code quality is high -- clean architecture, well-separated
> concerns, and strong alignment between design document and implementation. No critical
> bugs found. The findings below are organized by severity.

---

## Table of Contents

- [Input \& Event System Code Review](#input--event-system-code-review)
  - [Table of Contents](#table-of-contents)
  - [Summary](#summary)
  - [Medium Severity](#medium-severity)
    - [1. Lifetime: EventBus ScopedConnection Captures Raw Pointer](#1-lifetime-eventbus-scopedconnection-captures-raw-pointer)
    - [2. Robustness: EventBus Dispatch Bookkeeping Is Not Exception-Safe](#2-robustness-eventbus-dispatch-bookkeeping-is-not-exception-safe)
    - [3. Lifetime: InputRecorder / InputReplaySession Can Outlive InputDeviceManager](#3-lifetime-inputrecorder--inputreplaysession-can-outlive-inputdevicemanager)
    - [4. Robustness: InputRecording Binary Load Trusts Unbounded Counts from Disk](#4-robustness-inputrecording-binary-load-trusts-unbounded-counts-from-disk)
    - [5. Value Safety: Window Callbacks Forward Invalid Raw GLFW Codes](#5-value-safety-window-callbacks-forward-invalid-raw-glfw-codes)
    - [6. Design Boundary: Gamepad Sources Have No Capture Gate](#6-design-boundary-gamepad-sources-have-no-capture-gate)
    - [7. Fragile Init: InputNames Map Initialization Pattern](#7-fragile-init-inputnames-map-initialization-pattern)
    - [8. Semantics: ComboTracker Timeout Allows Same-Frame Restart](#8-semantics-combotracker-timeout-allows-same-frame-restart)
  - [Low Severity](#low-severity)
    - [9. Performance: String-Keyed Maps in Per-Frame Action Evaluation](#9-performance-string-keyed-maps-in-per-frame-action-evaluation)
    - [10. Performance: Cross-Context Queries Rebuild Blocked Sources Each Call](#10-performance-cross-context-queries-rebuild-blocked-sources-each-call)
    - [11. Const Semantics: Smooth Modifier Uses Mutable State](#11-const-semantics-smooth-modifier-uses-mutable-state)
    - [12. Behavior: New Subscribers May Fire During Active Publish](#12-behavior-new-subscribers-may-fire-during-active-publish)
    - [13. Missing Implementation: ReplayDevice Source Files Not Present](#13-missing-implementation-replaydevice-source-files-not-present)
    - [14. Redundancy: ScopedConnection Double-Wraps Heap Allocation](#14-redundancy-scopedconnection-double-wraps-heap-allocation)
  - [Style](#style)
    - [15. Structured Binding Void Casts](#15-structured-binding-void-casts)
    - [16. AxisEntry Default Value for Inactive Field](#16-axisentry-default-value-for-inactive-field)
  - [Strengths](#strengths)

---

## Summary

| Category | Count |
|----------|-------|
| Critical | 0 |
| Medium (lifetime, robustness, value safety, design boundary, fragile init, semantics) | 8 |
| Low (performance, const semantics, behavior, missing files, redundancy) | 6 |
| Style | 2 |

The core architecture (polling/event split, device slot model, action mapping with
modifier/trigger pipeline, context stack with chord blocking, observer-based
recording/replay) is solid and matches the design document faithfully. The main issues
concentrate around ownership contracts that are currently implicit rather than enforced
in code, and a few robustness gaps at system boundaries.

---

## Medium Severity

### 1. Lifetime: EventBus ScopedConnection Captures Raw Pointer

**File**: `src/Core/Event/EventBus.h:36-47`

```cpp
return ScopedConnection([this, id]()
{
    auto& s = GetSubscribers<T>();
    for (auto& entry : s.Entries)
    {
        if (entry.Id == id)
        {
            entry.PendingRemoval = true;
            s.Dirty = true;
            break;
        }
    }
});
```

The unsubscribe lambda captures `this` (the `EventBus` pointer). If the `EventBus` is
destroyed before all outstanding `ScopedConnection` objects, their destructors will
dereference a dangling pointer.

In the current engine, the `EventBus` is owned by `Application` and outlives all
subscribers, so this is not an active bug. However, the contract is fragile: any use of a
locally-scoped `EventBus` (e.g. in tests or future subsystems) would silently introduce
undefined behavior.

**Recommendation**: move subscription state behind shared lifetime-managed storage, have
`EventBus` explicitly invalidate all outstanding connections in its destructor, or at
minimum document the required lifetime ordering.

---

### 2. Robustness: EventBus Dispatch Bookkeeping Is Not Exception-Safe

**File**: `src/Core/Event/EventBus.h:55-76`

```cpp
subs.DispatchDepth++;
for (size_t i = 0; i < subs.Entries.size(); ++i)
{
    auto &entry = subs.Entries[i];
    if (!entry.PendingRemoval)
        entry.Handler(event);
}
subs.DispatchDepth--;
```

If a subscriber throws an exception, the `DispatchDepth` decrement and deferred-compaction
path are skipped. This leaves the subscriber list in an inconsistent state:
`DispatchDepth` stays artificially elevated, deferred removals may never compact, and later
publishes observe corrupted bookkeeping.

**Recommendation**: guard `DispatchDepth` with a scope guard (RAII) so it is restored even
on exceptional exit. Alternatively, document that handlers must not throw.

---

### 3. Lifetime: InputRecorder / InputReplaySession Can Outlive InputDeviceManager

**Files**: `src/Core/Input/Replay/InputRecording.h:82-84`,
`src/Core/Input/Replay/InputRecording.cpp:295-312`

Both `InputRecorder` and `InputReplaySession` store a raw `InputDeviceManager *` and
auto-detach in their destructors:

```cpp
InputDeviceManager *m_Manager = nullptr; // Non-owning.
~InputRecorder() override { Detach(); }
// Detach() calls m_Manager->RemoveObserver(this);
```

Same lifetime shape as the EventBus issue: if the manager is destroyed before the
recorder/session, the destructor dereferences a dangling pointer.

**Recommendation**: make manager destruction responsible for detaching observers, store
registrations as tokens, or document and assert the destruction order.

---

### 4. Robustness: InputRecording Binary Load Trusts Unbounded Counts from Disk

**File**: `src/Core/Input/Replay/InputRecording.cpp:233-288`

The recording loader reads `frameCount` and per-frame `gamepadCount` directly from disk
and immediately reserves:

```cpp
recording.Frames.reserve(frameCount);
// ...
frame.Gamepads.reserve(gamepadCount);
```

Subsequent `ReadPod` / `ReadArray` bounds checks protect against buffer overread, but not
against memory pressure. A corrupted file advertising extreme counts can force very large
allocations before the parser fails.

**Recommendation**: validate counts against the remaining byte budget and against practical
limits before reserving.

---

### 5. Value Safety: Window Callbacks Forward Invalid Raw GLFW Codes

**File**: `src/Core/App/Window.cpp` (GLFW callbacks)

GLFW callbacks cast raw platform values directly into engine code types:

```cpp
self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), false });
```

GLFW can report `GLFW_KEY_UNKNOWN` (-1), which wraps to 65535 in unsigned `Key::Code`.
The polling side bounds-checks via `KeyboardDevice::GetInput`, but the event side does not.
Subscribers receive apparently valid but meaningless codes.

**Recommendation**: validate callback values before publishing. Drop or sentinel unknown
values.

---

### 6. Design Boundary: Gamepad Sources Have No Capture Gate

**File**: `src/Core/Input/Action/InputSource.cpp:42-51`

`InputSourceState::IsDown` and `WasDown` check capture flags for key and mouse sources,
but gamepad sources have no capture check. If a UI system needs to suppress gamepad input
while a modal is active, there is no mechanism.

Not a bug (design doc does not mention gamepad capture), but worth recording as a known
boundary.

**Recommendation**: add `Input::IsGamepadCaptured()` now (low cost), or document the
deferral.

---

### 7. Fragile Init: InputNames Map Initialization Pattern

**File**: `src/Core/Input/Code/InputNames.cpp:102-122`

```cpp
static std::unordered_map<Key::Code, std::string> map;
if (map.empty())
{
    for (const auto &e : g_KeyTable)
        map[e.code] = e.name;
}
```

C++11 guarantees thread-safe construction of the static local, but the empty-check and
population happen after construction. Two threads calling concurrently for the first time
could both see `empty() == true` and race on population.

Design doc states main-thread-only, so not an active bug. But the pattern is fragile.

**Recommendation**: initialize in a lambda at construction time:

```cpp
static const auto map = []() {
    std::unordered_map<Key::Code, std::string> m;
    for (const auto &e : g_KeyTable)
        m[e.code] = e.name;
    return m;
}();
```

---

### 8. Semantics: ComboTracker Timeout Allows Same-Frame Restart

**File**: `src/Core/Input/Action/InputPatterns.cpp:14-25`

When a combo times out, `Reset()` sets `m_CurrentStep` to 0. The code then immediately
checks whether the current frame's input matches step 0, meaning a timeout and a new combo
start can happen on the same frame.

This may be intentional (common in combo systems), but the behavior is undocumented.

**Recommendation**: add a comment. If same-frame restart is undesired, return early after
`Reset()`.

---

## Low Severity

### 9. Performance: String-Keyed Maps in Per-Frame Action Evaluation

**File**: `src/Core/Input/Action/InputAction.h:167-188`

`InputActionMap` uses seven `std::unordered_map<std::string, ...>` members. `Update()`
iterates and queries these every frame. At current binding counts (< 20 per map) this is
negligible.

Design doc explicitly acknowledges this trade-off.

**Recommendation**: no action needed. Consider string interning if profiling shows a
hotspot in the future.

---

### 10. Performance: Cross-Context Queries Rebuild Blocked Sources Each Call

**File**: `src/Core/Input/Action/InputContextStack.cpp:74-93`

Each cross-context query allocates a fresh `vector<InputSource>` and walks the stack.
`Update()` already computes blocked sources but does not cache them for queries.

**Recommendation**: cache per-context blocked sources during `Update()`, or document the
O(contexts * queries) overhead as acceptable.

---

### 11. Const Semantics: Smooth Modifier Uses Mutable State

**File**: `src/Core/Input/Action/InputModifier.h:100-101`

`Smooth::Apply` is `const` per the base class, but mutates `mutable m_Smoothed`. This
breaks the const contract's implied promise. The root cause is that `Apply` was declared
`const` for stateless modifiers, but `Smooth` requires state.

**Recommendation**: make `Apply` non-const in the base class, which honestly reflects that
modifiers may be stateful. Minor concern.

---

### 12. Behavior: New Subscribers May Fire During Active Publish

**File**: `src/Core/Event/EventBus.h:58-63`

The publish loop checks `subs.Entries.size()` on each iteration. If a handler calls
`Subscribe<T>`, the new entry is appended, `size()` grows, and the new handler may fire
in the same `Publish` invocation.

**Recommendation**: snapshot entry count before the loop, or document the behavior.

---

### 13. Missing Implementation: ReplayDevice Source Files Not Present

**File**: design document section 10 (File Layout)

The design document lists `Replay/ReplayDevice.h / .cpp` containing
`ReplayKeyboardDevice`, `ReplayMouseDevice`, `ReplayGamepadDevice`. These files were not
found in the repository. `InputReplaySession` exposes frame data but the concrete replay
devices that would feed it into the device manager are absent.

**Recommendation**: implement the replay devices, or update the design doc file layout.

---

### 14. Redundancy: ScopedConnection Double-Wraps Heap Allocation

**File**: `src/Core/Event/ScopedConnection.h:14-16`

```cpp
explicit ScopedConnection(std::function<void()> unsub)
    : m_Unsubscribe(std::make_unique<std::function<void()>>(std::move(unsub)))
```

`std::function` already heap-allocates for non-trivial lambdas. Wrapping in
`unique_ptr<function>` adds a second allocation.

**Recommendation**: use a plain `std::function<void()>` member. Move semantics are
preserved, extra indirection eliminated.

---

## Style

### 15. Structured Binding Void Casts

**File**: `src/Core/Input/Action/InputAction.cpp:144-155`

Multiple `(void)variable;` casts to suppress unused structured binding warnings. Acceptable
as-is; `[[maybe_unused]]` on structured bindings is not supported until C++26.

---

### 16. AxisEntry Default Value for Inactive Field

**File**: `src/Core/Input/Action/InputAction.h:138`

`gamepadAxis = GamepadAxis::LeftX` default on a field that is only meaningful when
`kind == GamepadAxis`. Could confuse readers during debugging.

**Recommendation**: initialize to `0` with a comment, or leave as-is since the field is
only read for the correct `kind`.

---

## Strengths

| Aspect | Assessment |
|--------|------------|
| Architectural split | Polling state and discrete events are cleanly separated, matching the design doc. |
| Logical slot model | `InputDeviceManager` consistently resolves by `(Type, DeviceIndex)`. |
| Replay integration | Uses normal `InputDevice` abstraction and poll hooks -- no special-case path. |
| Input routing | `InputContextStack` with chord blocking is simple, readable, well-tested. |
| Processing pipeline | Modifier/trigger split mirrors Unreal Enhanced Input's proven architecture. |
| Serialization | `InputActionSerialization.h` is clean, human-readable JSON, forward-compatible. |
| Test coverage | Polling, actions, contexts, device manager, replay, triggers, modifiers, patterns, names, and event bus all have dedicated test files. |
