# Input System & Event System

A dual-subsystem input architecture for the RT Rendering Lab engine. The input side
provides polling-based per-frame state for gameplay and camera control. The event side
provides type-safe publish-subscribe delivery for discrete notifications such as window
resize, key press, and device lifecycle changes. The two paths share typed key, mouse,
and gamepad code definitions, but they solve different problems and are intentionally
kept separate.

> **Design Philosophy**: Polling-first for continuous control, event-driven for discrete
> notifications. Action mapping decouples gameplay code from physical bindings. Device
> abstraction separates live hardware from synthetic replay sources. Context routing
> resolves priority and ownership explicitly instead of relying on ad-hoc handled flags.

---

## Table of Contents

- [Input System \& Event System](#input-system--event-system)
  - [Table of Contents](#table-of-contents)
  - [1. Motivation](#1-motivation)
  - [2. Architecture Overview](#2-architecture-overview)
    - [2.1 Subsystem Split](#21-subsystem-split)
    - [2.2 Layered Model](#22-layered-model)
    - [2.3 Frame Flow](#23-frame-flow)
  - [3. Polling Input Foundation](#3-polling-input-foundation)
    - [3.1 Typed Codes](#31-typed-codes)
    - [3.2 `Input` Facade and Frame Lifecycle](#32-input-facade-and-frame-lifecycle)
    - [3.3 Query Semantics and Capture Flags](#33-query-semantics-and-capture-flags)
  - [4. Action Mapping](#4-action-mapping)
    - [4.1 `InputSource` and Binding Model](#41-inputsource-and-binding-model)
    - [4.2 Actions and Axes](#42-actions-and-axes)
    - [4.3 Serialization](#43-serialization)
  - [5. Processing Pipeline](#5-processing-pipeline)
    - [5.1 Modifiers](#51-modifiers)
    - [5.2 Triggers](#52-triggers)
    - [5.3 Advanced Patterns](#53-advanced-patterns)
  - [6. Context Routing](#6-context-routing)
    - [6.1 `InputContextStack`](#61-inputcontextstack)
    - [6.2 Chord Blocking Across Contexts](#62-chord-blocking-across-contexts)
    - [6.3 ImGui Routing](#63-imgui-routing)
  - [7. Device Abstraction and Lifecycle](#7-device-abstraction-and-lifecycle)
    - [7.1 `InputDevice` Contract](#71-inputdevice-contract)
    - [7.2 `InputDeviceManager`](#72-inputdevicemanager)
    - [7.3 Concrete Devices](#73-concrete-devices)
  - [8. Event Bus and Window Event Production](#8-event-bus-and-window-event-production)
    - [8.1 Event Bus Contract](#81-event-bus-contract)
    - [8.2 Event Types](#82-event-types)
    - [8.3 GLFW Callback Production](#83-glfw-callback-production)
  - [9. Recording and Replay](#9-recording-and-replay)
    - [9.1 Recording Model](#91-recording-model)
    - [9.2 Replay Runtime](#92-replay-runtime)
    - [9.3 Replay Boundary](#93-replay-boundary)
  - [10. File Layout](#10-file-layout)
  - [11. Key Design Decisions](#11-key-design-decisions)
    - [Why polling and events are separate](#why-polling-and-events-are-separate)
    - [Why `Input` remains a static facade](#why-input-remains-a-static-facade)
    - [Why action maps are string-keyed](#why-action-maps-are-string-keyed)
    - [Why context routing is stack-based](#why-context-routing-is-stack-based)
    - [Why replay uses device-manager observers](#why-replay-uses-device-manager-observers)
    - [Why replay excludes EventBus traffic](#why-replay-excludes-eventbus-traffic)
    - [Why ImGui still uses capture flags today](#why-imgui-still-uses-capture-flags-today)
  - [Appendix A: Industry Survey](#appendix-a-industry-survey)
    - [Unreal Engine Enhanced Input](#unreal-engine-enhanced-input)
    - [Unity Input System](#unity-input-system)
    - [Godot 4](#godot-4)
  - [Appendix B: Alternatives Considered](#appendix-b-alternatives-considered)
    - [One unified event-propagation input system](#one-unified-event-propagation-input-system)
    - [Raw device polling with no action layer](#raw-device-polling-with-no-action-layer)
    - [Per-event recording instead of per-frame snapshots](#per-event-recording-instead-of-per-frame-snapshots)
    - [Removing the `Input` facade entirely](#removing-the-input-facade-entirely)

---

## 1. Motivation

The original input path mixed direct GLFW polling, ad-hoc key code usage, and
window-specific callback logic. That was enough for simple demos, but it broke down as
soon as the engine needed:

- stable per-frame edge detection
- reusable action bindings shared across demos
- input priority between gameplay and modal UI
- device abstraction for gamepads and replay
- clean separation between "state that is true this frame" and "events that happened"

Modern engines converge on a common pattern: polling for continuous gameplay input,
typed action mapping above raw devices, and a separate event bus for notifications.
This system follows that pattern.

**What the system provides**:

- typed key, mouse, and gamepad codes with no GLFW dependency outside platform edges
- per-frame polling queries with current and previous state
- named action and axis bindings through `InputActionMap`
- modifier and trigger pipelines for processed values and timing-sensitive actions
- priority-based routing through `InputContextStack`
- keyboard, mouse, and gamepad abstraction through `InputDevice`
- discrete notifications through `EventBus`
- chord, combo, and double-tap support on top of the same binding model
- deterministic polling-state recording and replay through synthetic devices

---

## 2. Architecture Overview

### 2.1 Subsystem Split

The system is divided into two cooperating but independent paths:

| Subsystem | Purpose | Query Model | Examples |
|-----------|---------|-------------|----------|
| Input System | Continuous control state | Polling per frame | camera movement, mouse look, hold-to-sprint |
| Event System | Discrete notifications | Push / callback | resize, close, key pressed, device attached |

This distinction is deliberate. A key being held is state. A resize event is a
notification. Treating both as the same abstraction makes one side awkward.

### 2.2 Layered Model

The implementation is organized as a stack of increasingly higher-level behavior:

```text
Layer 10  Recording & Replay           synthetic polling sources, binary capture
Layer 9   Advanced Patterns            chords, combos, double-tap
Layer 8   ImGui Routing                capture flags bridge UI focus into polling
Layer 7   Window Event Production      GLFW callbacks -> EventBus
Layer 6   Event Bus                    type-safe publish/subscribe
Layer 5   Device Abstraction           keyboard, mouse, gamepad, replay devices
Layer 4   Context Routing              priority-based action-map stack
Layer 3   Processing Pipeline          modifiers and triggers
Layer 2   Action Mapping               named actions and axes
Layer 1   Polling State                per-frame current/previous device state
Layer 0   Typed Input Codes            key, mouse, gamepad identifiers
```

The important architectural boundary is between Layers 1-5, which define how input
state exists, and Layers 6-7, which define how discrete notifications are delivered.

### 2.3 Frame Flow

On a normal frame, the gameplay-facing polling path advances before layer updates:

```text
Window::PollEvents()
  -> GLFW callbacks
     |- EventBus publications
     `- Input::AccumulateScroll()

Time::Update()
Input::BeginFrame(dt)
  -> InputDeviceManager::PollAll(dt)
     |- optional replay advance before polling
     |- poll live or synthetic devices
     `- optional recording capture after polling

Layer updates
  -> Input facade queries
  -> InputActionMap / InputContextStack queries
  -> gameplay code
```

The event path remains synchronous and independent:

```text
GLFW callback -> Window -> EventBus::Publish(event) -> subscribers
```

---

## 3. Polling Input Foundation

### 3.1 Typed Codes

`KeyCode.h`, `MouseCode.h`, and `GamepadCode.h` define the engine's input identifiers.
They are intentionally available without pulling in GLFW headers.

`Key::Code` and `Mouse::Code` are lightweight numeric aliases rather than scoped enums.
That keeps call sites compact and avoids constant `static_cast` noise at the platform
boundary while still preserving named, engine-owned identifiers such as `Key::W` or
`Mouse::Right`.

`InputNames.h` provides bidirectional name mapping used by configuration and debugging:

- key name <-> `Key::Code`
- mouse button name <-> `Mouse::Code`
- gamepad button name <-> `GamepadButton::Code`
- gamepad axis name <-> `GamepadAxis::Code`

### 3.2 `Input` Facade and Frame Lifecycle

`Input` is the public polling facade used by most gameplay code. Internally it is no
longer the authoritative owner of runtime state; instead it delegates to the current
`InputDeviceManager`.

Core lifecycle entry points:

- `Initialize(GLFWwindow*)`
- `SetEventBus(EventBus*)`
- `BeginFrame(float dt)`
- `RegisterDevice(Scope<InputDevice>)`
- `RestoreDefaultDevices()`

`BeginFrame(dt)` is called once per frame from `Application::Run()`. That advances the
device manager and therefore the current/previous state used by all polling queries.

### 3.3 Query Semantics and Capture Flags

The polling API exposes the standard edge-detection trio for keyboard, mouse buttons,
and gamepad buttons:

```text
IsXDown(...)                 current frame is down
WasXPressedThisFrame(...)    current down, previous up
WasXReleasedThisFrame(...)   current up, previous down
```

Mouse state also exposes:

- absolute position
- delta since previous frame
- per-frame scroll delta accumulated from callbacks

ImGui integration currently uses capture flags:

- `SetKeyboardCaptured(bool)`
- `SetMouseCaptured(bool)`

When a device is captured, gameplay-facing polling queries return `false` or `0.0f`.
Absolute mouse position is the exception; it remains readable even while the mouse is
captured because UI and editor tools may still need it for layout or visualization.

---

## 4. Action Mapping

### 4.1 `InputSource` and Binding Model

`InputActionMap` is the bridge between physical input and gameplay intent. The underlying
unit is `InputSource`, which can describe:

- keyboard keys
- mouse buttons
- gamepad buttons
- gamepad axes

All runtime source evaluation goes through the shared `InputSourceState` helpers, so
single-key actions, chords, combo tracking, and replay-backed devices all resolve input
through one path.

### 4.2 Actions and Axes

`InputActionMap` supports two binding families:

| Binding Type | Value Kind | Typical Uses |
|--------------|------------|--------------|
| Action | boolean/discrete | toggle debug mode, confirm, fire |
| Axis | float/continuous | movement, look, scroll-based zoom |

Action bindings support:

- one or more single sources with OR semantics
- optional chord bindings where all listed sources must be active
- optional triggers for timing-sensitive activation

Axis bindings support:

- positive/negative key pairs
- mouse delta axes
- mouse scroll
- gamepad axes

Typical usage:

```cpp
m_InputMap.BindAxis("MoveForward", Key::W, Key::S);
m_InputMap.BindAxis("LookX", InputActionMap::MouseAxis::X);
m_InputMap.BindAction("ToggleLookMode", Mouse::Right);
m_InputMap.BindChordAction("Save", { InputSource::FromKey(Key::LeftControl),
                                     InputSource::FromKey(Key::S) });

m_InputMap.Update(dt);

const float forward = m_InputMap.GetAxis("MoveForward");
if (m_InputMap.WasActionTriggeredThisFrame("Save"))
    SaveScene();
```

### 4.3 Serialization

`InputActionSerialization.h` persists action-map bindings through the engine's shared
serialization layer.

Design goals of the serialization format:

- human-readable key names for source-controlled defaults
- compatibility with existing single-source action entries
- support for chord bindings without forcing a new file format

Configuration files live under the engine's config search path:

- shipped defaults under `Content/Config/`
- writable copies under `Saved/Config/`

---

## 5. Processing Pipeline

### 5.1 Modifiers

Modifiers transform raw axis values before gameplay reads them. They are applied in the
order they were added.

Built-in modifiers:

| Modifier | Purpose |
|----------|---------|
| `DeadZone` | suppress tiny analog or mouse noise |
| `Sensitivity` | scale input magnitude |
| `Clamp` | constrain to a min/max range |
| `Negate` | invert sign |
| `Smooth` | ease values over time |

This keeps sensitivity, inversion, and filtering logic out of demo-specific code.

### 5.2 Triggers

Triggers control when a discrete action becomes "triggered". Without an explicit trigger,
an action defaults to pressed-edge behavior.

Built-in triggers:

| Trigger | Fires when |
|---------|------------|
| `PressedTrigger` | source transitions from up to down |
| `ReleasedTrigger` | source transitions from down to up |
| `HoldTrigger(duration)` | source remains held long enough |
| `TapTrigger(maxDuration)` | source is pressed and released quickly |
| `DoubleTapTrigger(maxGap)` | second press arrives within the allowed gap |

Triggers are stateful and advance during `InputActionMap::Update(dt)`. Query APIs such
as `WasActionTriggeredThisFrame()` read the trigger cache rather than recomputing timing
logic ad hoc.

### 5.3 Advanced Patterns

Advanced patterns sit above the same `InputSource` abstraction rather than creating a
parallel gesture system.

**Chords**

`ChordBinding` requires all sources to be active simultaneously. Chords are integrated
into the action map directly rather than being modeled as an unrelated helper.

**Combos**

`ComboTracker` advances through a `ComboBinding` sequence when each step's source is
pressed within its allowed time window.

**Double-tap**

`DoubleTapTrigger` lives in the normal trigger pipeline, which means a double-tap action
can be bound and queried exactly like other triggered actions.

---

## 6. Context Routing

### 6.1 `InputContextStack`

`InputContextStack` coordinates multiple action maps through a priority-sorted stack of
named contexts. It solves the common problem where more than one layer binds the same
physical input but only one should currently respond.

Each context entry includes:

- a stable name
- a non-owning pointer to an `InputActionMap`
- a priority
- an active flag
- a `ConsumesInput` flag

The stack updates from highest priority to lowest. Cross-context queries such as
`IsActionDown()`, `WasActionTriggeredThisFrame()`, and `GetAxis()` return the first
reachable context that defines the requested binding.

### 6.2 Chord Blocking Across Contexts

Chord-aware routing is a key part of the current design. When a higher-priority context
has an engaged chord such as `Ctrl+S`, the lower-priority stack must not interpret the
shared key `Ctrl` as a standalone action.

The implementation handles this by collecting blocked sources from higher-priority
engaged chords and passing them into lower-priority action-map evaluation. This produces
implicit chord blocking without introducing a separate global key-mask system.

### 6.3 ImGui Routing

ImGui currently feeds keyboard and mouse focus into the polling path through the capture
flags on `Input`, not through a dedicated context entry. That keeps the implementation
simple and works for current needs, but it has two known limits:

- EventBus subscribers still receive discrete events even when ImGui has focus.
- Capture flags are updated from `ImGuiLayer::Begin()`, so gameplay observes the previous
  frame's capture decision rather than a same-frame routing change.

The current document treats that as an explicit integration boundary, not as hidden
behavior.

---

## 7. Device Abstraction and Lifecycle

### 7.1 `InputDevice` Contract

`InputDevice` is the engine's runtime abstraction for physical or synthetic input sources.

```cpp
struct InputValue
{
    float X = 0.0f;
    float Y = 0.0f;
};

class InputDevice
{
public:
    enum class Type : uint8_t { Keyboard, Mouse, Gamepad, Touch, Custom };

    virtual Type GetType() const = 0;
    virtual void Poll() = 0;

    virtual InputValue GetInput(uint16_t code) const = 0;
    virtual InputValue GetPreviousInput(uint16_t code) const { return {}; }

    virtual InputValue GetAxis(uint16_t axisId) const { return {}; }
    virtual InputValue GetPreviousAxis(uint16_t axisId) const { return {}; }

    virtual bool IsConnected() const { return true; }
    virtual bool WasConnected() const { return IsConnected(); }
    virtual bool HasConnectionStateChanged() const { return false; }
    virtual uint8_t GetDeviceIndex() const { return 0; }
    virtual void Reset() {}
};
```

This contract is intentionally broad enough to cover both live devices and replay-backed
devices without requiring a second interface.

### 7.2 `InputDeviceManager`

`InputDeviceManager` owns the active logical device slots and polls them once per frame.

Responsibilities:

- add, replace, remove, or clear devices
- resolve devices by `(Type, DeviceIndex)` logical slot
- reset all devices
- publish device lifecycle events through `EventBus`
- notify optional observers around poll boundaries

Observer hooks are used by recording and replay:

- `OnBeforePollAll(...)`
- `OnAfterPollAll(...)`

Those hooks keep recording/replay integrated with the device manager instead of
hard-coding special cases into `Input` or `Application`.

### 7.3 Concrete Devices

Current concrete device implementations:

| Device | Responsibility |
|--------|----------------|
| `KeyboardDevice` | polls GLFW key state and preserves previous-frame edges |
| `MouseDevice` | polls buttons, position, delta, and scroll |
| `GamepadDevice` | wraps GLFW gamepad state and connected/disconnected tracking |
| `ReplayKeyboardDevice` | feeds recorded keyboard state |
| `ReplayMouseDevice` | feeds recorded mouse state and derived deltas |
| `ReplayGamepadDevice` | feeds recorded gamepad buttons and axes |

The manager uses a slot model rather than an insertion-order model. For example:

- keyboard always occupies the keyboard slot
- mouse always occupies the mouse slot
- gamepads are addressed by logical index `0..3`

This is what makes replay and hot-swapping predictable.

---

## 8. Event Bus and Window Event Production

### 8.1 Event Bus Contract

`EventBus` handles discrete notifications only. It is intentionally not used for
continuous gameplay polling.

Core properties:

- type-safe subscription via `Subscribe<T>(handler)`
- RAII unsubscription through `ScopedConnection`
- stack-allocated event payloads passed as `const T&`
- synchronous dispatch
- safe deferred removal during nested or in-flight dispatch

Typical usage:

```cpp
m_ResizeConnection = m_EventBus.Subscribe<WindowResizeEvent>(
    [this](const WindowResizeEvent& e)
    {
        OnWindowResize(e.Width, e.Height);
    });
```

### 8.2 Event Types

Current event families:

| Family | Events |
|--------|--------|
| Window | `WindowResizeEvent`, `WindowCloseEvent` |
| Keyboard | `KeyPressedEvent`, `KeyReleasedEvent`, `CharTypedEvent` |
| Mouse | `MouseButtonPressedEvent`, `MouseButtonReleasedEvent`, `MouseScrolledEvent` |
| Device lifecycle | `DeviceAttachedToSlotEvent`, `DeviceDetachedFromSlotEvent`, `DeviceConnectionChangedEvent` |
| Gamepad compatibility | `GamepadConnectedEvent`, `GamepadDisconnectedEvent` |
| Application | `DemoSwitchedEvent` |

The device lifecycle split is deliberate:

- attach/detach describes slot topology changes
- connection-changed describes whether the device in a slot is usable
- gamepad connected/disconnected is a compatibility projection for consumers that only
  care about gamepads

### 8.3 GLFW Callback Production

`Window::SetEventBus()` installs GLFW callbacks that publish engine events. Those
callbacks currently cover:

- framebuffer resize
- window close
- key press / release / repeat
- character input
- mouse button press / release
- scroll

Scroll is the one callback that intentionally feeds both systems:

- `MouseScrolledEvent` goes to the EventBus
- `Input::AccumulateScroll()` feeds the polling path

That dual path is correct because scroll has both discrete and polling consumers.

---

## 9. Recording and Replay

### 9.1 Recording Model

Recording captures per-frame polling state, not raw platform events. The persisted unit
is `InputFrame`, which stores:

- frame number
- delta time
- keyboard presence, connection, and key state
- mouse presence, connection, buttons, position, and scroll delta
- zero or more logical gamepad slot snapshots

`InputRecorder` attaches to `InputDeviceManager` as an observer and captures frames
after device polling completes. Recordings are written to a compact versioned binary
format through `SaveToFile(...)` and restored through `LoadFromFile(...)`.

### 9.2 Replay Runtime

Replay is built from two pieces:

| Component | Responsibility |
|-----------|----------------|
| `InputReplaySession` | owns recorded frames, advances the current frame, tracks playback state |
| replay devices | expose the session's current and previous frame state through the normal `InputDevice` API |

`InputReplaySession` advances in `OnBeforePollAll(...)`, which means replay devices
already see the right "current frame" when the device manager begins querying them.

Replay devices preserve normal semantics:

- current vs previous button queries
- mouse position and derived delta
- gamepad axis values
- connected vs previously connected state

The session also tracks whether observed runtime `dt` deviates from recorded `dt`,
allowing deterministic playback validation without aborting playback immediately.

### 9.3 Replay Boundary

Replay deliberately covers only the polling path. It does **not** attempt to serialize
or reproduce EventBus traffic such as:

- `CharTypedEvent`
- `WindowResizeEvent`
- device attach/detach publications

Those remain live runtime concerns. This boundary is intentional: the current replay
system is designed for deterministic gameplay input reproduction, not for full platform
simulation.

---

## 10. File Layout

```text
src/
|-- Core/
|   |-- Input/
|   |   |-- Input.h / Input.cpp
|   |   |-- Action/
|   |   |   |-- InputAction.h / .cpp
|   |   |   |-- InputActionSerialization.h
|   |   |   |-- InputContextStack.h / .cpp
|   |   |   |-- InputModifier.h
|   |   |   |-- InputTrigger.h
|   |   |   |-- InputSource.h / .cpp
|   |   |   `-- InputPatterns.h / .cpp
|   |   |-- Code/
|   |   |   |-- KeyCode.h
|   |   |   |-- MouseCode.h
|   |   |   |-- GamepadCode.h
|   |   |   `-- InputNames.h / .cpp
|   |   |-- Device/
|   |   |   |-- InputDevice.h
|   |   |   |-- InputDeviceManager.h / .cpp
|   |   |   |-- KeyboardDevice.h / .cpp
|   |   |   |-- MouseDevice.h / .cpp
|   |   |   `-- GamepadDevice.h / .cpp
|   |   `-- Replay/
|   |       |-- InputRecording.h / .cpp
|   |       `-- ReplayDevice.h / .cpp
|   |-- Event/
|   |   |-- EventBus.h
|   |   |-- Events.h
|   |   `-- ScopedConnection.h
|   `-- App/
|       |-- Application.h / .cpp
|       `-- Window.h / .cpp
|
|-- GUI/
|   `-- ImGuiLayer.h / ImGuiLayer.cpp
|
`-- Demos/
    `-- ... demo-owned action maps and consumers

tests/
|-- Unit/
|   |-- TestInput.cpp
|   |-- TestInputAction.cpp
|   |-- TestInputContextStack.cpp
|   |-- TestInputDeviceManager.cpp
|   |-- TestEventBus.cpp
|   |-- TestInputModifiers.cpp
|   |-- TestInputNames.cpp
|   |-- TestInputPatterns.cpp
|   |-- TestInputRecording.cpp
|   `-- TestInputTriggers.cpp
`-- Integration/
    `-- TestInputRecordingIO.cpp
```

The input and event modules live under `Core/` because they are engine infrastructure
used by demos, GUI, and platform-facing app code alike.

---

## 11. Key Design Decisions

### Why polling and events are separate

Continuous control is easier to reason about as state. Discrete notifications are easier
to reason about as events. Combining them forces one of the two to use an unnatural
shape. The split also keeps gameplay code free of event queue plumbing.

### Why `Input` remains a static facade

Most gameplay code wants concise queries such as `Input::IsKeyDown(...)` or
`Input::GetMouseDelta()`. Keeping the facade preserves that ergonomics, while moving the
authoritative state behind `InputDeviceManager` keeps the architecture extensible.

### Why action maps are string-keyed

Action names need to be readable in config files and demo code. The lookup cost is tiny
relative to frame work, while the readability and serialization benefits are substantial.

### Why context routing is stack-based

A priority-ordered stack models modal UI, gameplay, and tool overlays directly. It also
keeps ownership explicit: the first reachable context that defines an action wins. This
is easier to reason about than a distributed "handled" protocol across many listeners.

### Why replay uses device-manager observers

Replay and recording need to hook poll boundaries, not random gameplay code. Observer
hooks let them integrate at the one place where device state advances, without teaching
`Input` or `Application` about every replay concern.

### Why replay excludes EventBus traffic

Full platform-event replay is a much larger problem than deterministic gameplay input
reproduction. By restricting replay to polling state, the engine gets a stable and
useful feature without inventing a partial, misleading event-replay contract.

### Why ImGui still uses capture flags today

The current capture-flag bridge is simple and effective for present needs. A full
ImGui-as-context integration is possible later, but forcing it early would add coupling
to the context stack before the editor/UI layer actually needs that complexity.

---

## Appendix A: Industry Survey

### Unreal Engine Enhanced Input

| Unreal Concept | This Engine |
|----------------|-------------|
| `UInputMappingContext` | `InputActionMap` + `InputContextStack` |
| `UInputAction` | named action entries |
| `UInputModifier` | `InputModifier` subclasses |
| `UInputTrigger` | `InputTrigger` subclasses |
| mapping-context priority | context-stack priority |
| chord / combo support | chord bindings + `ComboTracker` + `DoubleTapTrigger` |

Unreal's strongest influence here is the layered split between raw devices, named
actions, modifiers/triggers, and context priority.

### Unity Input System

| Unity Concept | This Engine |
|---------------|-------------|
| `InputActionAsset` | serializable action-map configuration |
| processors | modifiers |
| interactions | triggers |
| `InputDevice` | `InputDevice` |
| action-map enable/disable | `InputContextStack::SetActive()` |

Unity's data-driven binding model strongly validates keeping action definitions readable
and serializable rather than burying raw keys in gameplay code.

### Godot 4

| Godot Concept | This Engine |
|---------------|-------------|
| `InputMap` | `InputActionMap` |
| `Input.is_action_pressed()` | `IsActionDown()` |
| `_input(event)` | EventBus subscriptions |
| per-action dead zone | `DeadZone` modifier |
| input recording via custom tooling | polling-state recording and replay |

Godot is the clearest reminder that a lightweight engine can still benefit from a clean
polling/event split without replicating a heavyweight enterprise framework.

---

## Appendix B: Alternatives Considered

### One unified event-propagation input system

This would route all input through event objects and propagation rules. It looks
conceptually pure, but it makes held-state queries, edge detection, and action-map
evaluation much more awkward. The engine would still end up reconstructing per-frame
state somewhere.

### Raw device polling with no action layer

Direct `Key::W` and `Mouse::Right` checks are fine for toy examples, but they do not
scale across demos, remapping, config files, gamepads, or replay-backed devices.
`InputActionMap` is the point where intent becomes stable.

### Per-event recording instead of per-frame snapshots

Recording raw events seems more precise, but it introduces ordering questions around
callbacks, repeat events, and engine frame boundaries. Polling-state snapshots align
with how gameplay code actually consumes input and therefore replay more naturally.

### Removing the `Input` facade entirely

Forcing all callers to hold or retrieve `InputDeviceManager` and query devices directly
would be architecturally strict but ergonomically poor. The facade is intentionally
retained for common gameplay access, while advanced systems still work directly with the
underlying device and action-map layers.
