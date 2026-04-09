# Input System & Event System

A dual-subsystem input architecture for the RT Rendering Lab engine. The Input System
provides polling-based per-frame state queries for continuous control; the Event System
provides a type-safe publish-subscribe bus for discrete notifications. Both share typed
key and mouse code constants but serve fundamentally different consumers.

> **Design Philosophy**: Polling-first for continuous input, event-driven for discrete
> notifications. Input Action Mapping decouples game logic from physical keys.
> Signal/Slot (publish-subscribe) replaces inheritance-based event dispatch.
> Modular layers - each layer is independently testable and depends only on layers below it.

---

## Table of Contents

- [Input System \& Event System](#input-system--event-system)
  - [Table of Contents](#table-of-contents)
  - [1. System Overview](#1-system-overview)
    - [Full Architecture Stack](#full-architecture-stack)
  - [2. Layer 0 - Key \& Mouse Codes](#2-layer-0---key--mouse-codes)
  - [3. Layer 1 - Input Device State (Polling)](#3-layer-1---input-device-state-polling)
  - [4. Layer 2 - Input Action Map](#4-layer-2---input-action-map)
  - [5. Layer 3 - Input Modifiers \& Triggers](#5-layer-3---input-modifiers--triggers)
    - [Modifiers](#modifiers)
    - [Triggers](#triggers)
  - [6. Layer 4 - Input Context Stack](#6-layer-4---input-context-stack)
  - [7. Layer 5 - Input Device Abstraction](#7-layer-5---input-device-abstraction)
    - [Current interfaces](#current-interfaces)
  - [8. Layer 6 - Event Bus (Signal/Slot)](#8-layer-6---event-bus-signalslot)
    - [Event types](#event-types)
  - [9. Layer 7 - Window Event Production](#9-layer-7---window-event-production)
  - [10. Layer 8 - ImGui Input Routing](#10-layer-8---imgui-input-routing)
  - [11. Layer 9 - Chord, Combo \& Gesture Recognition](#11-layer-9---chord-combo--gesture-recognition)
    - [Chord (simultaneous keys)](#chord-simultaneous-keys)
    - [Combo (sequential inputs)](#combo-sequential-inputs)
    - [Double-tap](#double-tap)
  - [12. Layer 10 - Input Recording \& Replay](#12-layer-10---input-recording--replay)
    - [Architecture](#architecture)
    - [Planned interfaces](#planned-interfaces)
    - [Determinism requirements](#determinism-requirements)
  - [13. Data Flow Diagram](#13-data-flow-diagram)
    - [Core flow (implemented layers)](#core-flow-implemented-layers)
    - [Full flow (all layers, including planned)](#full-flow-all-layers-including-planned)
  - [14. File Layout](#14-file-layout)
  - [15. Phased Implementation Plan](#15-phased-implementation-plan)
    - [Phase A - Foundation](#phase-a---foundation)
    - [Phase B - ImGui integration \& event system](#phase-b---imgui-integration--event-system)
    - [Phase C - Action mapping](#phase-c---action-mapping)
    - [Phase D - Resize callback unification](#phase-d---resize-callback-unification)
    - [Phase E - Input processing pipeline (Modifiers \& Triggers)](#phase-e---input-processing-pipeline-modifiers--triggers)
    - [Phase F - Input Context Stack](#phase-f---input-context-stack)
    - [Phase G - Device abstraction](#phase-g---device-abstraction)
    - [Phase H - Advanced input patterns](#phase-h---advanced-input-patterns)
    - [Phase I - Recording \& replay](#phase-i---recording--replay)
    - [Implementation order summary](#implementation-order-summary)
  - [16. Appendix A: Comparison with Cherno-Style](#16-appendix-a-comparison-with-cherno-style)
  - [17. Appendix B: Reference - How Production Engines Do It](#17-appendix-b-reference---how-production-engines-do-it)
    - [Unreal Engine - Enhanced Input](#unreal-engine---enhanced-input)
    - [Unity - Input System](#unity---input-system)
    - [Godot 4](#godot-4)

---

## 1. System Overview

The system is split into two independent subsystems that share key/mouse code types
but serve fundamentally different purposes:

| Subsystem | Purpose | Query Model | Examples |
|-----------|---------|-------------|----------|
| **Input System** | Continuous control state | Polling per frame | Camera movement, look-around, hold-to-sprint |
| **Event System** | Discrete notifications | Push (callback) | Window resize, window close, key-just-pressed once, demo switched |

**Why two subsystems instead of one unified Event system?**

Continuous input (camera WASD, mouse look) is naturally a per-frame state query.
Wrapping every key poll in an event object adds allocation overhead, complicates
the control flow, and makes debugging harder. Modern engines (Unreal Enhanced Input,
Unity Input System, Godot) all treat continuous and discrete input differently.

Events are reserved for things that "happen" rather than things that "are":
a window resizes (happens), a key is held down (is).

### Full Architecture Stack

```text
Layer 10  Input Recording & Replay         (injectable input source)             planned
Layer 9   Chord / Combo / Gesture          (state machines on top of actions)    planned
Layer 8   ImGui Input Routing              (capture flags block polling)         partial
Layer 7   Window Event Production          (GLFW callbacks -> EventBus)          implemented
Layer 6   Event Bus (Signal/Slot)          (type-safe pub/sub, RAII connections) implemented
Layer 5   Input Device Abstraction         (keyboard, mouse, gamepad as devices) implemented
Layer 4   Input Context Stack              (priority-based action map activation) implemented
Layer 3   Input Modifiers & Triggers       (dead zone, sensitivity, hold/tap)    implemented
Layer 2   Input Action Map                 (logical actions -> physical bindings) implemented
Layer 1   Input Device State (Polling)     (double-buffered per-frame state)     implemented
Layer 0   Key & Mouse Codes                (typed constants, no GLFW dependency) implemented
```

Layers 0-7 form the current implemented core, with the capture-flag portion of Layer 8
bridging ImGui focus into the polling path. Layers 9 and 10 remain planned extensions.
Each layer depends only on layers below it.

**Serialization**: `InputActionMap` bindings persist through the shared serialization
framework (`Serialization::SaveToConfigPath` / `LoadFromConfigPath` +
`InputActionSerialization.h`). Config files live in `Content/Config/` (shipped
defaults) and are auto-copied into `Saved/Config/` on first access through the
resource system's config fallback chain.

**`InputSource::Type`** includes keyboard, mouse, gamepad button, and gamepad axis
sources, and runtime querying now resolves all of them through Layer 5's device manager.

---

## 2. Layer 0 - Key & Mouse Codes

`KeyCode.h` and `MouseCode.h` provide typed constants (`Key::W`, `Mouse::Left`,
`Key::LeftControl`, etc.) that eliminate raw `int` key codes and remove GLFW header
dependencies from all code outside `Input.cpp` and `Window.cpp`.

Key codes use `Key::Code = uint16_t` rather than `enum class`. This allows implicit
conversion to `int` for GLFW calls without `static_cast` noise while still providing
named constants. Values are numerically identical to GLFW constants, so no translation
is needed at the GLFW boundary. Namespace scope (`Key::`, `Mouse::`) keeps call sites
short (`Key::W` vs `KeyCode::Key::W`).

Because these are not generic named enum types, `InputNames.h` provides bidirectional
`Key::Code` to string mapping for serialization and debug display, rather than the
`magic_enum`-backed path used by newer internal `enum class` types.

---

## 3. Layer 1 - Input Device State (Polling)

`Input` is a static class that snapshots GLFW keyboard and mouse state once per frame
into double-buffered arrays. `Application::Run()` calls `Input::BeginFrame()` at the
top of the frame loop, before iterating layers.

**Keyboard and mouse buttons** are polled via `glfwGetKey` / `glfwGetMouseButton` into
a "current" buffer; the previous frame's buffer is retained. This enables three query
modes per key:

```
IsKeyDown(key)               - held right now (current buffer)
WasKeyPressedThisFrame(key)  - current=true, previous=false
WasKeyReleasedThisFrame(key) - current=false, previous=true
```

The same triple is provided for mouse buttons. **Mouse position and delta** are tracked
via `glfwGetCursorPos`, with the first-frame edge case handled to prevent a spurious
large delta on startup. **Scroll** is the one exception to polling: GLFW only reports
it via callback, so the scroll callback feeds an accumulator that `BeginFrame()` drains
each frame and exposes via `GetScrollDelta()`.

**Capture flags** (`SetKeyboardCaptured` / `SetMouseCaptured`) are set by `ImGuiLayer`
each frame. All polling query methods return `false` / `0.0f` immediately when the
corresponding device is captured, so game/demo code requires no explicit ImGui checks
at call sites.

`Initialize(GLFWwindow*)` must be called once before any query method. Calling it again
with the same non-null window is a no-op.

---

## 4. Layer 2 - Input Action Map

`InputActionMap` decouples game/demo logic from physical key bindings via two
abstractions:

- **Actions** (discrete, boolean): named events like "ShowShadowMap" or "ToggleLookMode".
  Any number of sources can be bound to one action (OR logic - any one triggers it).
- **Axes** (continuous, float): named values like "MoveForward" or "LookX". Bound to
  either a positive/negative key pair (returns `[-1, +1]`) or a mouse delta/scroll axis
  (raw pixel or scroll units).

Each demo/layer owns its own `InputActionMap` instance. The `ShadowMapping` and
`MaterialPlayground` demos each load bindings from `Content/Config/input/*.json` and
fall back to hardcoded defaults saved to `Saved/Config/`.
`InputSource::Type` includes gamepad button and gamepad axis values, 
and `IsSourceDown()` / edge queries now resolve
them through `InputDeviceManager`.

**Typical usage**:

```cpp
// OnAttach - register bindings
m_InputMap.BindAxis("MoveForward", Key::W, Key::S);
m_InputMap.BindAxis("LookX",       InputActionMap::MouseAxis::X);
m_InputMap.BindAction("ShowShadowMap",  Key::D2);
m_InputMap.BindAction("ToggleLookMode", Mouse::Right);

// OnUpdate - query results
float forward = m_InputMap.GetAxis("MoveForward");
if (m_InputMap.WasActionPressedThisFrame("ShowShadowMap"))
    m_OutputMode = SceneRendererOutput::ShadowMap;
if (m_InputMap.IsActionDown("ToggleLookMode"))
    m_CameraController.OnMouseDelta(m_InputMap.GetAxis("LookX"), ...);
```

**Design decisions**:

- String-keyed for readability. `unordered_map` lookup overhead is negligible compared
  to draw calls.
- `GetAxis()` returns `float` in `[-1, +1]` for key pairs, or raw delta for mouse. The
  consumer decides how to scale it - unless Layer 3 modifiers are applied.
- Bindings can be registered in code or loaded from JSON. Key name serialization flows
  through `InputNames.h`; internal named enums use `magic_enum`-backed helpers.

**Known limitation**: The API models actions as `bool` and axes as `float`. If the
long-term goal includes gamepad sticks or composite movement vectors (like Unreal's
`FInputActionValue` with 1D/2D/3D value types), this will require a later API redesign.

---

## 5. Layer 3 - Input Modifiers & Triggers

Layer 3 adds a configurable processing pipeline between raw input values and game logic,
inspired by Unreal Enhanced Input. Both `InputModifier.h` and `InputTrigger.h` are
header-only. 17 unit tests cover modifiers, triggers, chaining, timing, and reset
behavior.

### Modifiers

Without modifiers, sensitivity scaling and dead-zone logic are duplicated across every
demo. With modifiers, the action map handles the full pipeline:

```cpp
m_InputMap.BindAxis("LookX", InputActionMap::MouseAxis::X);
m_InputMap.AddModifier("LookX", std::make_unique<DeadZone>(0.01f));
m_InputMap.AddModifier("LookX", std::make_unique<Sensitivity>(0.12f));
m_InputMap.AddModifier("LookX", std::make_unique<Clamp>(-10.0f, 10.0f));
// GetAxis("LookX") now returns the fully processed value.
```

Modifiers are applied in insertion order (Raw -> Modifier1 -> Modifier2 -> ... -> Final).
Five built-in modifiers: `DeadZone`, `Sensitivity`, `Clamp`, `Negate`, `Smooth`.
Custom modifiers subclass `InputModifier` and implement `Apply(float value, float dt)`.

`Smooth` uses `mutable` for its internal smoothed value because `Apply` is const from
the caller's perspective.

Camera controller sensitivity has not yet been migrated into modifiers; deferred until
an input-settings UI is built.

### Triggers

A trigger controls **when** an action transitions to "triggered" state. Without a
trigger, an action fires the instant its source is pressed. Four built-in triggers:

| Trigger | Fires when |
|---------|-----------|
| `PressedTrigger` | Source transitions to pressed (default) |
| `HoldTrigger(duration)` | Source held for `duration` seconds (fires once) |
| `TapTrigger(maxDuration)` | Pressed and released within `maxDuration` seconds |
| `ReleasedTrigger` | Source transitions to released |

Triggers are stateful and must receive `Update(dt)` each frame. `InputActionMap`
exposes `AddModifier()`, `SetTrigger()`, `Update(dt)`, `WasActionTriggeredThisFrame()`,
and `GetActionTriggerState()` (returns `None`, `Ongoing`, or `Triggered`).

**Backward compatibility**: `GetAxis()` returns the cached modified value if `Update(dt)`
was called, otherwise falls back to raw computation. `WasActionTriggeredThisFrame()`
falls back to `WasActionPressedThisFrame()` when no trigger is set. Existing demos work
unchanged without calling `Update(dt)`.

---

## 6. Layer 4 - Input Context Stack

`InputContextStack` coordinates multiple `InputActionMap` instances through a
priority-sorted vector with consumption logic. Without it, two layers that both bind
`Escape` - a pause menu and gameplay - fire on the same frame with no way to resolve
priority. The context stack solves this:

```cpp
contextStack.Push("Gameplay", &gameplayMap, 0);

// When pause menu opens:
contextStack.Push("PauseMenu", &pauseMenuMap, 100, /*consumesInput=*/ true);
// "Gameplay" bindings are now blocked.

// When pause menu closes:
contextStack.Pop("PauseMenu");
// "Gameplay" resumes. No stale trigger state.
```

**Update behavior**: `Update(dt)` iterates contexts from highest to lowest priority,
calling `Update(dt)` on each active ActionMap. A context with `ConsumesInput=true`
stops iteration - lower-priority contexts receive neither updates nor query responses.
An inactive consuming context does not block.

**Cross-context queries** (`IsActionDown`, `WasActionTriggeredThisFrame`, `GetAxis`)
walk the priority-sorted list and return the result from the first active, reachable
context that defines the requested action or axis. A consuming context also gates query
reachability for lower-priority contexts.

**Runtime state reset**: `SetActive(name, false)` and `Pop(name)` both call
`ActionMap::ResetRuntimeState()` before deactivating or removing the context. This
prevents stale trigger state - a `HoldTrigger` mid-progress will not fire spuriously
when the context is re-enabled later.

**Design decisions**:

- Non-owning pointers to ActionMaps. The stack is a coordinator, not an owner.
- `ConsumesInput` is a simple boolean, not per-action. A modal UI either blocks all
  game input or it doesn't. Per-action exclusion lists can be added later.
- Stable sort by priority - equal-priority contexts maintain insertion order.
- Duplicate names replace the existing entry after resetting its runtime state.

**Real engine parallels**:

| Engine | Equivalent Concept |
|--------|--------------------|
| Unreal | `UInputMappingContext` + priority parameter in `AddMappingContext()` |
| Unity  | `InputActionMap.Enable()` / `Disable()` + `PlayerInput` action map stack |
| Godot  | `InputMap` + `set_process_input()` enable/disable |

**Remaining work**: ImGui has not yet been integrated as the highest-priority consuming
context. Currently ImGui uses capture flags directly on the `Input` polling layer (Layer 8).
A future step would push ImGui as a `consumesInput=true` context and retire the separate
flag mechanism.

---

## 7. Layer 5 - Input Device Abstraction

Layer 5 is now the runtime boundary between physical or synthetic input sources and the
rest of the gameplay-facing polling stack. `Input` remains as a convenience facade, but
its queries are backed by an `InputDeviceManager` that owns logical device slots for the
keyboard, mouse, and gamepads. `InputActionMap` also resolves sources through the device
manager rather than calling keyboard/mouse-only `Input::` helpers.

This abstraction is the prerequisite for Layer 10 (Recording/Replay), where a future
`ReplayDevice` can inject recorded state through the same `InputDevice` contract used by
live devices.

### Current interfaces

```cpp
struct InputValue { float X = 0.0f; float Y = 0.0f; };   // buttons: X = 1.0 or 0.0

class InputDevice
{
public:
    enum class Type : uint8_t { Keyboard, Mouse, Gamepad, Touch, Custom };
    virtual Type       GetType()  const = 0;
    virtual void       Poll()           = 0;   // called once per frame
    virtual InputValue GetInput(uint16_t code)  const = 0;
    virtual InputValue GetPreviousInput(uint16_t code) const { return {}; }
    virtual InputValue GetAxis(uint16_t axisId) const { return {}; }
    virtual InputValue GetPreviousAxis(uint16_t axisId) const { return {}; }
    virtual bool       IsConnected()    const { return true; }
    virtual bool       WasConnected()   const { return IsConnected(); }
    virtual bool       HasConnectionStateChanged() const { return false; }
    virtual uint8_t    GetDeviceIndex() const { return 0; }
    virtual void       Reset() {}
};

class InputDeviceManager
{
public:
    void         AddDevice(std::unique_ptr<InputDevice> device);
    void         Clear();
    void         PollAll();    // called once per frame by Application
    void         ResetAll();
    InputDevice* GetDevice(InputDevice::Type type, uint8_t index = 0) const;
    std::unique_ptr<InputDevice> RemoveDevice(InputDevice::Type type, uint8_t index = 0);
    void         SetEventBus(EventBus* bus);
};
```

Current concrete implementations:

- `KeyboardDevice` - polls GLFW key state and preserves previous-frame edges.
- `MouseDevice` - polls buttons and cursor position, exposes delta and scroll axes.
- `GamepadDevice` - wraps `glfwJoystickIsGamepad()` / `glfwGetGamepadState()` and
  tracks connected state plus previous-frame button/axis data.

With the abstraction in place, `InputActionMap::IsSourceDown()` / pressed / released
queries all resolve through `InputDeviceManager`, and gamepad source types are fully
functional at runtime.

**Design decisions**:

- `InputValue` is a small struct (two floats), not a variant - avoids branching on type.
- Devices are polled, not callback-driven, matching the polling-first philosophy.
- `Input` is intentionally kept as a convenience facade for common gameplay queries, but
  it no longer owns the authoritative state directly.
- `RestoreDefaultDevices()` now means a true restore of the default live layout
  (keyboard, mouse, gamepad slots 0-3). Extra custom devices or non-default slots are
  removed instead of silently retained.

---

## 8. Layer 6 - Event Bus (Signal/Slot)

`EventBus` is a lightweight publish-subscribe system for discrete, system-level
notifications. It is not used for per-frame input polling. `Application` owns the bus,
`Window` publishes GLFW-backed events into it, and `LabLayer` publishes `DemoSwitchedEvent`.

**Design principles**:

1. **Type-safe dispatch**: subscribe for a specific event type `T`, receive `const T&`.
   No base class, no virtual dispatch, no downcasting.
2. **RAII lifetime**: `Subscribe<T>` returns a `ScopedConnection` that auto-unsubscribes
   on destruction. Storing the connection as a member is sufficient lifetime management.
3. **No global state**: `EventBus` is an instance owned by `Application`, passed by
   pointer where needed.
4. **No allocation per event**: events are stack-constructed and passed by reference.
5. **Safe during dispatch**: subscribe/unsubscribe during `Publish()` uses deferred
   removal via a `DispatchDepth` counter, preventing iterator invalidation.

**Subscription pattern**:

```cpp
m_OnResize = m_EventBus->Subscribe<WindowResizeEvent>(
    [this](const WindowResizeEvent& e)
    {
        m_Camera.SetViewportSize(e.Width, e.Height);
    });
// m_OnResize is a ScopedConnection member - unsubscribes automatically on destruction.
```

**Known limitation**: `ScopedConnection` captures `EventBus* this` in its unsubscribe
callable. If a connection outlives the owning `EventBus`, destroying the connection
dereferences a dangling pointer. The current architecture keeps `EventBus` lifetime tied
to `Application` (which lives longest), but clear lifetime rules should be enforced as
the codebase grows.

### Event types

Events are plain structs - no base class, no macros, no inheritance hierarchy. Current
event types:

- **Window**: `WindowResizeEvent { Width, Height }`, `WindowCloseEvent`
- **Keyboard**: `KeyPressedEvent { KeyCode, IsRepeat }`, `KeyReleasedEvent { KeyCode }`,
  `CharTypedEvent { Codepoint }`
- **Mouse**: `MouseButtonPressedEvent { Button }`, `MouseButtonReleasedEvent { Button }`,
  `MouseScrolledEvent { XOffset, YOffset }`
- **Device lifecycle**: `DeviceAttachedToSlotEvent { DeviceType, DeviceIndex }`,
  `DeviceDetachedFromSlotEvent { DeviceType, DeviceIndex }`,
  `DeviceConnectionChangedEvent { DeviceType, DeviceIndex, Connected }`
- **Gamepad compatibility**: `GamepadConnectedEvent { DeviceIndex }`,
  `GamepadDisconnectedEvent { DeviceIndex }`
- **Application**: `DemoSwitchedEvent { NewIndex, NewName }`

Device lifecycle semantics are intentionally split:

- `DeviceAttachedToSlotEvent` / `DeviceDetachedFromSlotEvent` describe logical slot
  topology changes in `InputDeviceManager`.
- `DeviceConnectionChangedEvent` describes whether a slot's current device is connected
  and usable.
- `GamepadConnectedEvent` / `GamepadDisconnectedEvent` are compatibility projections of
  `DeviceConnectionChangedEvent` for `Type::Gamepad`; they do not imply slot attach or
  detach by themselves.

**Design decisions**:

- No `Handled` flag. If ImGui needs to block input from reaching game logic, that
  happens at the Input System level (capture flags), not by stopping event propagation.
- Synchronous dispatch. `Publish()` calls handlers inline. Appropriate for current scale.

**Planned extensions**:

- `RendererSettingsChangedEvent` - for propagating `DebugPanel` tweaks; fields TBD.
- **Event queue**: ring buffer stores events, `FlushEvents()` dispatches at a defined
  point in the frame. Useful for deterministic ordering and cross-thread scenarios.

---

## 9. Layer 7 - Window Event Production

`Window::SetEventBus()` installs GLFW callbacks that publish events into the bus.
Callbacks cover: framebuffer resize, window close, key press/release/repeat, character
input, mouse button press/release, and scroll.

The scroll callback feeds both the EventBus (`MouseScrolledEvent`) and
`Input::AccumulateScroll()` simultaneously. This is the one case where both paths are
active - event subscribers receive the raw value, polling consumers read the per-frame
accumulated total via `Input::GetScrollDelta()`.

`Window::SetRefreshCallback()` provides a separate path for macOS live-resize rendering.
On macOS, GLFW blocks the main loop during window resize, so a refresh callback triggers
`Application::RenderFrame()` to keep the display responsive. This is a render concern
rather than an event notification concern; the two paths coexist intentionally.

---

## 10. Layer 8 - ImGui Input Routing

When ImGui wants keyboard or mouse focus, `ImGuiLayer::Begin()` forwards
`ImGuiIO::WantCaptureKeyboard` and `WantCaptureMouse` to `Input::SetKeyboardCaptured()`
and `Input::SetMouseCaptured()`. All `Input` polling query methods return `false` / `0.0f`
when the corresponding device is captured, so game/demo code requires no explicit ImGui
checks at call sites.

**Why capture flags instead of event-based `Handled` flags?** One flag per device,
checked in one place, with no ordering dependency on which layer processes events first.
ImGui's `WantCapture*` flags are already the authoritative source - we forward them directly.

**Current limitations**:

- The EventBus path is not blocked by capture flags. Discrete events (e.g., `KeyPressedEvent`)
  still reach all subscribers even when ImGui has focus.
- Capture flags are updated in `ImGuiLayer::Begin()`, which runs after `OnUpdate()`.
  Gameplay polling therefore observes the previous frame's capture state rather than a
  same-frame routing decision.

**Future refinement**: When Layer 4 integration is complete, ImGui can be pushed as the
highest-priority consuming context and the separate flag mechanism retired.

---

## 11. Layer 9 - Chord, Combo & Gesture Recognition

**Planned extension.** Layer 9 adds multi-key combinations, sequential inputs, and
time-sensitive gestures as first-class action bindings. All are built on top of the
existing `InputActionMap` + `InputTrigger` infrastructure - no separate system needed.

### Chord (simultaneous keys)

A chord is "all of these keys must be held at the same time":

```cpp
struct ChordBinding
{
    std::vector<InputSource> Sources;   // ALL must be active simultaneously

    bool IsActive() const
    {
        return std::all_of(Sources.begin(), Sources.end(),
            [](const InputSource& s) { return IsSourceDown(s); });
    }
};
```

**Key subtlety**: When `Ctrl+S` is bound as a chord, pressing `Ctrl` alone should NOT
trigger any action bound to just `Ctrl`. This requires the context stack to check "is
this key part of a pending chord?" before firing single-key actions. Unreal handles this
with **implicit chord blocking**.

### Combo (sequential inputs)

A combo is "press these keys in this order within a time window":

```cpp
struct ComboBinding
{
    struct Step { InputSource Source; float MaxDelay; };
    std::vector<Step> Steps;
};

class ComboTracker
{
public:
    void SetCombo(const ComboBinding& combo);
    bool Update(float dt);   // Returns true on the frame the full combo completes.
    void Reset();
private:
    ComboBinding m_Combo;
    size_t m_CurrentStep = 0;
    float  m_StepTimer = 0.0f;
};
```

### Double-tap

```cpp
class DoubleTapTrigger : public InputTrigger
{
public:
    explicit DoubleTapTrigger(float maxGap = 0.3f);
    TriggerState Evaluate(bool, bool pressed, bool, float dt) override;
    void Reset() override;
private:
    float m_MaxGap;
    float m_GapTimer = 0.0f;
    bool  m_WaitingForSecond = false;
};
```

**Dependency**: Requires Phase E (triggers) and Phase F (context stack for chord blocking).

---

## 12. Layer 10 - Input Recording & Replay

**Planned extension.** Layer 10 captures per-frame input state to a file and replays it
deterministically - useful for automated testing, bug reproduction, and demo playback.

### Architecture

With the Device Abstraction (Layer 5), recording and replay become a matter of swapping
device implementations:

```
Normal:   GLFWwindow -> KeyboardDevice -> Input -> ActionMap -> Game
Record:   GLFWwindow -> KeyboardDevice -> RecordingTee -> Input -> ActionMap -> Game
                                         ->                                    Recording File
Replay:   Recording File -> ReplayDevice -> Input -> ActionMap -> Game
```

### Planned interfaces

```cpp
struct InputFrame
{
    uint64_t               FrameNumber;
    float                  DeltaTime;
    std::vector<Key::Code> KeysDown;         // Compressed: only keys that are down
    float                  MouseX, MouseY;
    float                  MouseDeltaX, MouseDeltaY;
    float                  ScrollDelta;
    uint8_t                MouseButtonsDown; // Bitmask of Mouse::Button0..Button7
};

class InputRecorder
{
public:
    void BeginRecording();
    void EndRecording();
    void CaptureFrame(uint64_t frameNumber, float dt);
    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);
};

// Implements InputDevice; feeds recorded state instead of live input.
class ReplayDevice : public InputDevice { ... };
```

### Determinism requirements

1. **Fixed timestep** or recorded delta times must be replayed exactly.
2. **No external randomness** in input-dependent code paths (or seed must be recorded).
3. **Frame-accurate state** - all input queries must go through the device abstraction;
   no direct GLFW calls in game logic.

**Design decisions**:

- Per-frame state snapshots, not per-event. Matches polling-first architecture and
  avoids timing issues with callback ordering.
- Binary format for compactness. A 60 fps recording for 1 minute is ~3600 frames,
  roughly 100 KB uncompressed.

**Known design gap**: The replay design covers polling-based gameplay input state but
the boundary relative to discrete engine events (`CharTypedEvent`, `WindowResizeEvent`,
device connect/disconnect) is not yet defined.

**Dependency**: Phase G is complete; `ReplayDevice` can build directly on the existing `InputDevice` contract.

---

## 13. Data Flow Diagram

### Core flow (implemented layers)

```text
GLFW callbacks -> Window / Input bridge
  |- scroll callback -> Input::AccumulateScroll()
  `- discrete callbacks -> EventBus

Application::Run()
  -> Input::BeginFrame()
  -> InputDeviceManager::PollAll()
  -> Input facade queries
  -> InputActionMap
  -> Modifiers / Triggers
  -> InputContextStack
  -> Demo / Layer code
```

### Full flow (all layers, including planned)

```text
Platform / GLFW
  |                         \
  | poll live devices        \ discrete callbacks
  v                           v
InputDeviceManager (Layer 5)  EventBus (Layer 6)
  |- KeyboardDevice           |- WindowResize / WindowClose
  |- MouseDevice              |- Key / Mouse / Char events
  |- GamepadDevice            |- DeviceAttachedToSlot / DetachedFromSlot
  |- (future) ReplayDevice    |- DeviceConnectionChanged
  |                           |- GamepadConnected / Disconnected
  v                           |- DemoSwitched
Input (Layer 1)               |
  v                           |
InputActionMap (Layer 2)      |
  v                           |
Modifiers & Triggers (Layer 3)|
  v                           |
InputContextStack (Layer 4) <-> ImGui capture flags (Layer 8, partial)
  v
Game / Demo code

Planned sidecars after Layer 5:
- Layer 9: chord / combo / gesture recognition
- Layer 10: input recording + replay via synthetic InputDevice implementations
```

**Key insight**: The Input polling path (left) and Event bus path (right) are
**independent**. They share GLFW as the source but serve different consumers.
Game logic uses the left path. System notifications use the right path.

---

## 14. File Layout

```text
src/
|-- Core/
|   |-- Input/
|   |   |-- Input.h / Input.cpp                    # Layer 1    implemented
|   |   |-- Action/
|   |   |   |-- InputAction.h / .cpp               # Layer 2    implemented
|   |   |   |-- InputActionSerialization.h         # Layer 2    implemented
|   |   |   |-- InputModifier.h                    # Layer 3    implemented
|   |   |   |-- InputTrigger.h                     # Layer 3    implemented
|   |   |   `-- InputContextStack.h / .cpp         # Layer 4    implemented
|   |   |-- Code/
|   |   |   |-- KeyCode.h                          # Layer 0    implemented
|   |   |   |-- MouseCode.h                        # Layer 0    implemented
|   |   |   |-- GamepadCode.h                      # Layer 0/5  implemented
|   |   |   `-- InputNames.h / .cpp                # Layer 2/5  implemented
|   |   `-- Device/
|   |       |-- InputDevice.h                      # Layer 5    implemented
|   |       |-- KeyboardDevice.h / .cpp            # Layer 5    implemented
|   |       |-- MouseDevice.h / .cpp               # Layer 5    implemented
|   |       |-- GamepadDevice.h / .cpp             # Layer 5    implemented
|   |       `-- InputDeviceManager.h / .cpp        # Layer 5    implemented
|   |-- Event/
|   |   |-- ScopedConnection.h                     # Layer 6    implemented
|   |   |-- EventBus.h                             # Layer 6    implemented
|   |   `-- Events.h                               # Layer 6    implemented
|   `-- App/
|       |-- Window.h / Window.cpp                  # Layer 7    implemented
|       `-- Application.h / Application.cpp        # main loop wiring
|
|-- GUI/
|   `-- ImGuiLayer.h / ImGuiLayer.cpp              # Layer 8    partial
|
`-- Demos/
    `-- ...                                        # demo consumers of the input stack

tests/
|-- Unit/
|   |-- TestInput.cpp                              # Layers 1 + 5
|   |-- TestInputAction.cpp                        # Layers 2 + 5
|   |-- TestInputNames.cpp                         # code/name mapping
|   |-- TestInputModifiers.cpp                     # Layer 3
|   |-- TestInputTriggers.cpp                      # Layer 3
|   |-- TestInputContextStack.cpp                  # Layer 4
|   `-- TestInputDeviceManager.cpp                 # Layer 5 lifecycle + events
`-- Contract/
    `-- Core/TestEventBus.cpp                      # Layer 6
```

---

## 15. Phased Implementation Plan

Phases A through G are complete. Phases H-I remain in dependency order.

### Phase A - Foundation

Implemented `KeyCode.h`, `MouseCode.h`, revised `Input` signatures from raw `int` to
typed codes, added `BeginFrame()` double-buffer, and `WasKeyPressedThisFrame`. This
eliminated `constexpr int KEY_W = 87` hacks from demo code and fixed one-shot toggle
behavior.

### Phase B - ImGui integration & event system

Implemented `ScopedConnection`, `EventBus`, `Events`, window callback production via
`InstallCallbacks()`, `Input` capture flags, `Window::SetRefreshCallback()` (macOS
live-resize), and `Application::RenderFrame()` extraction. `Application` now subscribes
to `WindowResizeEvent` via EventBus; `Window::SetResizeCallback` / `m_ResizeCallback`
have been removed. Two notification paths remain: EventBus (discrete events) and
RefreshCallback (macOS live-resize render path - a different concern).

| Item | Layer | Status |
|------|-------|--------|
| `ScopedConnection.h` | 6 | Done |
| `EventBus.h` (deferred removal) | 6 | Done |
| `Events.h` (struct definitions) | 6 | Done |
| Window callback installation | 7 | Done |
| `Input` capture flags | 8 | Done |
| `Window::SetRefreshCallback` | 7 | Done |
| `Application::RenderFrame()` extraction | - | Done |
| HiDPI framebuffer size init | 7 | Done |
| Migrate resize to EventBus, remove `m_ResizeCallback` | 7 | Done |

### Phase C - Action mapping

Implemented `InputActionMap` (actions + axes, OR-binding, keyboard/mouse/delta/scroll),
`DemoSwitchedEvent`, and `InputActionSerialization.h`. Demos reorganized into
`demos/showcase/` and `demos/tutorial/`. `RendererSettingsChangedEvent` is still a
placeholder; fields TBD based on `SceneRendererSpecification`.

| Item | Layer | Status |
|------|-------|--------|
| `InputActionMap` | 2 | Done |
| `InputActionSerialization.h` | 2 | Done |
| `DemoSwitchedEvent` | 6 | Done |
| `RendererSettingsChangedEvent` | 6 | Placeholder only |

### Phase D - Resize callback unification

Unified resize notification to EventBus-only. `Window::SetResizeCallback` /
`m_ResizeCallback` removed. The redundant `glfwSetFramebufferSizeCallback` call in
`Window::Init()` removed (superseded by `InstallCallbacks()`). Verified on Windows.

### Phase E - Input processing pipeline (Modifiers & Triggers)

Implemented `InputModifier` (5 built-ins), `InputTrigger` (4 built-ins),
`InputActionMap::AddModifier()` / `SetTrigger()` / `Update(dt)` /
`WasActionTriggeredThisFrame()` / `GetActionTriggerState()`. 17 unit tests.
Backward-compatible with existing demos.

| Step | Layer | Description | Status |
|------|-------|-------------|--------|
| 1. `InputModifier` base + built-ins | 3 | `DeadZone`, `Sensitivity`, `Clamp`, `Negate`, `Smooth` | Done |
| 2. `InputTrigger` base + built-ins | 3 | `PressedTrigger`, `HoldTrigger`, `TapTrigger`, `ReleasedTrigger` | Done |
| 3. `AddModifier()` / `SetTrigger()` | 2+3 | Integrated into `InputActionMap` | Done |
| 4. `Update(dt)` | 3 | Advances trigger FSMs; called once per frame | Done |
| 5. `WasActionTriggeredThisFrame()` / `GetActionTriggerState()` | 3 | Trigger-aware query API | Done |
| 6. Migrate camera controller sensitivity | 3 | `DebugCameraController` no longer owns scaling | Deferred |
| 7. Unit tests | 3 | 8 modifier tests + 9 trigger tests | Done |

### Phase F - Input Context Stack

Implemented `InputContextStack` with priority-sorted contexts, consumption blocking,
`SetActive()` with runtime state reset, and cross-context queries. 17 unit tests
covering priority ordering, consumption blocking, activate/deactivate, and stale-state
prevention.

| Step | Layer | Description | Status |
|------|-------|-------------|--------|
| 1. `InputContextStack` class | 4 | Priority-sorted vector of `InputContext` entries | Done |
| 2. `Push()` / `Pop()` / `SetActive()` | 4 | Context lifecycle with runtime state reset | Done |
| 3. `Update(dt)` with consumption logic | 4 | Higher-priority contexts block lower ones | Done |
| 4. Cross-context query methods | 4 | `IsActionDown()`, `GetAxis()`, `WasActionTriggeredThisFrame()` | Done |
| 5. ImGui as highest-priority context | 4+8 | Replace capture flags with a context entry | Deferred |
| 6. Unit tests | 4 | Priority ordering, consumption blocking, reset behavior | Done |

### Phase G - Device abstraction

Implemented. Layer 5 now owns the authoritative runtime polling state for keyboard,
mouse, and gamepad slots, and `InputActionMap` resolves all source types through
`InputDeviceManager`.

| Step | Layer | Description | Status |
|------|-------|-------------|--------|
| 1. `InputDevice` interface | 5 | `Poll()`, `GetInput()`, `GetAxis()`, previous-frame queries, connection state | Done |
| 2. `KeyboardDevice` + `MouseDevice` | 5 | Extracted polling state into device instances | Done |
| 3. `InputDeviceManager` | 5 | Owns devices, calls `PollAll()` once per frame, supports add/remove/clear | Done |
| 4. Refactor `InputActionMap` source queries | 5 | Query through device manager instead of keyboard/mouse-only paths | Done |
| 5. `GamepadDevice` | 5 | Wraps `glfwGetGamepadState()` and joystick connected state | Done |
| 6. Device lifecycle events | 5+6 | Slot attach/detach plus connection-changed events, with gamepad compatibility events | Done |
| 7. Decide fate of `Input` static API | 5 | Kept as convenience facade backed by the device manager | Done |

Remaining follow-up work for this area lives in Phase I:

- `ReplayDevice` and recording infrastructure
- final documentation of event ordering relative to replay capture/playback
- any future migration from capture flags to ImGui-as-context

### Phase H - Advanced input patterns

**Not started.** Implement when game mechanics demand chord shortcuts, sequential combos,
or double-tap gestures.

| Step | Layer | Description |
|------|-------|-------------|
| 1. `ChordBinding` | 9 | All sources must be active simultaneously |
| 2. Chord blocking in context stack | 9+4 | Suppress single-key actions while chord is pending |
| 3. `ComboTracker` | 9 | State machine for sequential input patterns |
| 4. `DoubleTapTrigger` | 9+3 | Fires on second press within time window |

**Dependency**: Phase E (triggers) and Phase F (context stack for chord blocking).

### Phase I - Recording & replay

**Not started.** Implement when automated testing or demo playback is needed.

| Step | Layer | Description |
|------|-------|-------------|
| 1. `InputRecorder` | 10 | Capture per-frame input snapshots to `InputFrame` |
| 2. `ReplayDevice` | 10 | `InputDevice` implementation that feeds recorded state |
| 3. Binary save/load | 10 | Persist recordings to disk (compact format) |
| 4. Deterministic replay validation | 10 | Verify frame-accurate reproduction |

**Dependency**: Phase G is complete; Layer 10 can build on the existing `InputDevice`
contract and `InputDeviceManager` slot model.

### Implementation order summary

```
Phase A  Foundation                 Done
Phase B  ImGui / Event System       Done
Phase C  Action Mapping             Done (RendererSettingsChangedEvent deferred)
Phase D  Resize unification         Done
Phase E  Modifiers & Triggers       Done (camera migration deferred)
Phase F  Context Stack              Done (ImGui-as-context deferred)
Phase G  Device Abstraction         Done
Phase H  Advanced Patterns          -> after F + G
Phase I  Recording & Replay         -> after G
```

Phases H-I form the remaining dependency chain.

---

## 16. Appendix A: Comparison with Cherno-Style

| Aspect | Cherno-style (Illusion) | This design |
|--------|------------------------|-------------|
| Event base class | `class Event` with virtual functions | No base class; plain structs |
| Dispatch mechanism | `EventDispatcher` + `static_cast` | `EventBus::Subscribe<T>` type-safe template |
| Category filtering | `EventCategory` bitfield + `IsInCategory()` | Not needed; subscribe to exactly what you want |
| Macros | `EVENT_CLASS_TYPE`, `EVENT_CLASS_CATEGORY` | None |
| Input routing | Events propagate through LayerStack | Polling + capture flags + context stack |
| Lifetime management | Manual (must remember to stop propagation) | RAII `ScopedConnection` |
| Continuous input | Mixed with event system | Separate polling system with `InputActionMap` |
| Allocation | Heap-allocated event objects | Stack-constructed, passed by `const&` |
| Dispatch safety | No protection against mid-dispatch mutation | Deferred removal with `DispatchDepth` counter |
| Input abstraction | Direct GLFW calls | Device abstraction layer (implemented) |
| Action mapping | None (raw key codes in game logic) | `InputActionMap` with modifiers and triggers |
| Multi-layer input | LayerStack linear propagation | `InputContextStack` with priorities |

---

## 17. Appendix B: Reference - How Production Engines Do It

### Unreal Engine - Enhanced Input

| Concept | UE Implementation | Our Equivalent |
|---------|-------------------|----------------|
| `UInputMappingContext` | Maps actions to keys, with priority | `InputActionMap` + `InputContextStack` |
| `UInputAction` | Defines a named action with value type | Action entries in `InputActionMap` |
| `UInputModifier` | DeadZone, Negate, Scalar, FOVScaling, etc. | `InputModifier` subclasses |
| `UInputTrigger` | Down, Pressed, Released, Hold, Tap, Combo, ChordAction | `InputTrigger` subclasses |
| `AddMappingContext(ctx, priority)` | Pushes context with priority | `InputContextStack::Push()` |
| Implicit chord blocking | Higher-priority chord blocks lower single-key | `InputContextStack` chord awareness (planned) |

### Unity - Input System

| Concept | Unity Implementation | Our Equivalent |
|---------|---------------------|----------------|
| `InputActionAsset` | JSON/YAML asset defining actions + bindings | `InputActionMap` (code-defined, serializable) |
| `InputAction.Enable()` / `Disable()` | Toggle action map | `InputContextStack::SetActive()` |
| `Interaction` | Hold, Tap, MultiTap, SlowTap, Press | `InputTrigger` |
| `Processor` | DeadZone, Normalize, Invert, Scale | `InputModifier` |
| `InputDevice` | Abstract device base class | `InputDevice` |
| `InputEventTrace` | Records input events to buffer | `InputRecorder` (planned) |

### Godot 4

| Concept | Godot Implementation | Our Equivalent |
|---------|---------------------|----------------|
| `InputMap` | Global action to key mapping | `InputActionMap` |
| `Input.is_action_pressed()` | Polling query | `InputActionMap::IsActionDown()` |
| `Input.is_action_just_pressed()` | Edge detection | `InputActionMap::WasActionPressedThisFrame()` |
| `_input(event)` / `_unhandled_input(event)` | Event propagation through scene tree | `EventBus` + context stack |
| `InputEventAction` | Action-level event | `Subscribe<ActionTriggeredEvent>` (future) |
| Dead zone | Per-action configurable | `DeadZone` modifier |



