# Input & Event System Internals

This document describes how the input and event systems actually work at the code level,
and compares RTRLab's approach with the input systems found in modern game engines.

It is intended as a companion to the main
[Design.md](Design.md) design document, which focuses on the public
contract and design rationale. This document focuses on implementation mechanics and
industry context.

> **Snapshot date**: 2026-04-09

---

## Table of Contents

- [Input \& Event System Internals](#input--event-system-internals)
  - [Table of Contents](#table-of-contents)
  - [Part I: How the System Works](#part-i-how-the-system-works)
    - [1. End-to-End Frame Flow](#1-end-to-end-frame-flow)
    - [2. Typed Input Codes (Layer 0)](#2-typed-input-codes-layer-0)
    - [3. Input Facade and Device Delegation](#3-input-facade-and-device-delegation)
    - [4. Device Abstraction Layer](#4-device-abstraction-layer)
      - [4.1 KeyboardDevice](#41-keyboarddevice)
      - [4.2 MouseDevice](#42-mousedevice)
      - [4.3 GamepadDevice](#43-gamepaddevice)
      - [4.4 InputDeviceManager](#44-inputdevicemanager)
    - [5. Action Mapping](#5-action-mapping)
      - [5.1 InputSource and State Resolution](#51-inputsource-and-state-resolution)
      - [5.2 InputActionMap Update Cycle](#52-inputactionmap-update-cycle)
    - [6. Processing Pipeline](#6-processing-pipeline)
      - [6.1 Modifiers](#61-modifiers)
      - [6.2 Triggers](#62-triggers)
    - [7. Context Routing](#7-context-routing)
      - [7.1 Update Flow with Chord Blocking](#71-update-flow-with-chord-blocking)
      - [7.2 Cross-Context Queries](#72-cross-context-queries)
    - [8. Advanced Patterns](#8-advanced-patterns)
    - [9. EventBus](#9-eventbus)
    - [10. Recording and Replay](#10-recording-and-replay)
      - [10.1 Recording](#101-recording)
      - [10.2 Replay](#102-replay)
      - [10.3 Binary Format](#103-binary-format)
    - [11. Key Implementation Details](#11-key-implementation-details)
  - [Part II: Comparison with Modern Game Engines](#part-ii-comparison-with-modern-game-engines)
    - [12. Polling vs Event Model](#12-polling-vs-event-model)
    - [13. Action Mapping and Context Routing](#13-action-mapping-and-context-routing)
    - [14. Device Abstraction](#14-device-abstraction)
    - [15. Processing Pipeline (Modifiers and Triggers)](#15-processing-pipeline-modifiers-and-triggers)
    - [16. Recording and Replay](#16-recording-and-replay)
    - [17. Mechanisms Present in Mature Engines but Absent in RTRLab](#17-mechanisms-present-in-mature-engines-but-absent-in-rtrlab)
    - [18. Overall Positioning](#18-overall-positioning)

---

## Part I: How the System Works

### 1. End-to-End Frame Flow

The system operates along two independent paths per frame: the **polling path** for
continuous gameplay state and the **event path** for discrete notifications.

```
Window::PollEvents()
  -> GLFW callbacks fire synchronously
     |- EventBus::Publish(KeyPressedEvent / MouseScrolledEvent / ...)
     `- Input::AccumulateScroll(yOffset)          [scroll feeds both paths]

Time::Update()

Input::BeginFrame(dt)
  -> InputDeviceManager::PollAll(dt)
       |- observers->OnBeforePollAll(manager, dt)   [replay advances here]
       |- device->Poll() for each device            [current/previous state updated]
       |- observers->OnAfterPollAll(manager, dt)    [recording captures here]
       `- publish DeviceConnectionChangedEvent for any connection changes

Layer updates (OnUpdate)
  -> Input::IsKeyDown / GetMouseDelta / ...         [facade queries]
  -> InputActionMap::Update(dt) / query APIs        [action-level queries]
  -> InputContextStack::Update(dt) / query APIs     [multi-context queries]
  -> gameplay code
```

The event path is synchronous and independent:

```
GLFW callback -> Window -> EventBus::Publish(event) -> all subscribers called inline
```

The sole point where both paths meet is **scroll**: the GLFW scroll callback both
publishes `MouseScrolledEvent` to the EventBus and feeds `Input::AccumulateScroll()` for
the polling path.

---

### 2. Typed Input Codes (Layer 0)

`KeyCode.h`, `MouseCode.h`, and `GamepadCode.h` define engine-owned input identifiers.

**Implementation** (`KeyCode.h:10-148`, `MouseCode.h:9-24`, `GamepadCode.h:10-57`):

- `Key::Code`, `Mouse::Code`, `GamepadButton::Code`, `GamepadAxis::Code` are all
  `uint16_t` aliases with named constants in anonymous enums.
- Key values match GLFW constants directly (e.g. `Key::Space = 32`, `Key::Escape = 256`),
  eliminating conversion at the platform boundary.
- No GLFW headers are included -- these files are safe to use anywhere in the engine.

**Name mapping** (`InputNames.h` / `InputNames.cpp`):

Bidirectional `Code <-> std::string` mapping via static `unordered_map` tables. Four
namespaces provide `ToName(code)` and `FromName(name)`:

- `Key::ToName(Key::W)` -> `"W"`
- `Mouse::FromName("Right")` -> `Mouse::Right (1)`
- `GamepadButton::ToName(GamepadButton::A)` -> `"A"`
- `GamepadAxis::FromName("LeftX")` -> `GamepadAxis::LeftX (0)`

Tables are initialized lazily on first access using a static-local pattern with
`constexpr` source tables.

---

### 3. Input Facade and Device Delegation

`Input` (`Input.h` / `Input.cpp`) is a static facade class. It holds no per-instance
state; all members are `static`.

**Internal delegation model**:

```
Input::IsKeyDown(key)
  -> if (s_KeyboardCaptured) return false
  -> GetDevice(InputDevice::Type::Keyboard)            [anonymous helper]
  -> device->GetInput(key).X > 0.5f                    [IsPressed threshold]
```

All polling queries follow the same pattern: check capture flag, resolve device from the
manager, evaluate the device's current or previous state.

**Capture flags** (`Input.cpp:228-244`):

- `s_KeyboardCaptured` and `s_MouseCaptured` are set by the ImGui layer.
- When captured, all gameplay-facing queries return `false` / `0.0f`.
- **Exception**: `GetMousePosition()` is never gated by capture, because UI code may still
  need absolute cursor position for layout.

**Device initialization** (`Input.cpp:248-267`):

`EnsureDevices(window)` lazily creates an `InputDeviceManager` and populates it with
default devices: one `KeyboardDevice`, one `MouseDevice`, and four `GamepadDevice`
instances (indices 0-3). If a slot is already populated, it is not replaced.

`RestoreDefaultDevices()` (`Input.cpp:61-80`) is more aggressive: it either creates a
fresh manager or clears the existing one, then re-creates all six default devices. This is
used after replay to restore the live device layout.

---

### 4. Device Abstraction Layer

#### 4.1 KeyboardDevice

`KeyboardDevice.h` / `KeyboardDevice.cpp`

- State: two `std::array<bool, 512>` -- `m_CurrentKeys` and `m_PreviousKeys`.
- `Poll()`: copies current to previous, then queries GLFW for keys `GLFW_KEY_SPACE`
  through `GLFW_KEY_LAST`.
- Edge detection: caller compares `GetInput(code).X` vs `GetPreviousInput(code).X`.
- `ApplyState(keys)`: used by test support to inject synthetic keyboard frames without
  GLFW.

#### 4.2 MouseDevice

`MouseDevice.h` / `MouseDevice.cpp`

- State: buttons (8 bools), position (X/Y), delta (computed), scroll accumulator.
- `Poll()` flow:
  1. Save previous delta: `m_PreviousDeltaX = m_MouseX - m_LastMouseX`
  2. Copy current buttons to previous
  3. Query GLFW for button state and cursor position
  4. Snapshot scroll: `m_ScrollThisFrame = m_ScrollAccumulator; m_ScrollAccumulator = 0`
- First-mouse-sample handling: on the very first poll, `m_LastMouseX/Y` is set to the
  current position to avoid a large initial delta spike.
- `GetAxis(axisId)` exposes five axes: `PositionX`, `PositionY`, `DeltaX`, `DeltaY`,
  `ScrollY`.
- `AccumulateScroll(yOffset)`: called from the GLFW scroll callback between polls,
  additive.

#### 4.3 GamepadDevice

`GamepadDevice.h` / `GamepadDevice.cpp`

- Addressed by `m_DeviceIndex` (0-3), mapped to GLFW joystick IDs via
  `GLFW_JOYSTICK_1 + deviceIndex`.
- `Poll()`:
  1. Save previous state
  2. Check `glfwJoystickIsGamepad(joystickId)`
  3. If connected, read `GLFWgamepadstate` for buttons and axes
  4. Track `m_HasConnectionStateChanged` using `m_HasPolledOnce` to prevent false-positive
     disconnect events on the first frame.

#### 4.4 InputDeviceManager

`InputDeviceManager.h` / `InputDeviceManager.cpp`

Slot-based device registry. Devices are identified by `(Type, DeviceIndex)`.

**AddDevice** (`InputDeviceManager.cpp:61-88`):

If a device with the same `(Type, Index)` already exists, the old one is replaced:
1. Publish `DeviceDetachedFromSlotEvent` for the old device
2. Replace the `Scope<InputDevice>` in the vector
3. Publish `DeviceAttachedToSlotEvent` for the new device
4. If connection state changed, publish `DeviceConnectionChangedEvent`

**PollAll** (`InputDeviceManager.cpp:102-130`):

```
1. Copy observer list (guards against mutation during callbacks)
2. observer->OnBeforePollAll(*this, dt)    [replay advances frame pointer here]
3. device->Poll() for each device
4. observer->OnAfterPollAll(*this, dt)     [recording captures frame here]
5. For each device with HasConnectionStateChanged():
     publish DeviceConnectionChangedEvent (and GamepadConnected/Disconnected for gamepads)
```

The observer copy in step 1 means that an observer adding/removing itself during a
callback will not affect the current iteration.

---

### 5. Action Mapping

#### 5.1 InputSource and State Resolution

`InputSource` (`InputSource.h`) is a lightweight value type: `{SourceType, Code,
DeviceIndex}`. Four factory methods: `FromKey`, `FromMouseButton`, `FromGamepadButton`,
`FromGamepadAxis`.

`InputSourceState` (`InputSource.cpp`) provides the shared state-resolution functions used
by all action evaluation:

```cpp
IsDown(source)       -> resolve device from manager -> device->GetInput(code).X > 0.5f
WasDown(source)      -> device->GetPreviousInput(code).X > 0.5f
WasPressedThisFrame  -> IsDown && !WasDown
WasReleasedThisFrame -> !IsDown && WasDown
```

Capture flags are checked inside `IsDown` / `WasDown` for Key and MouseButton types.
Gamepad sources are never captured.

Blocking: `IsBlocked(source, blockedSources)` does a linear scan comparing
`(SourceType, Code, DeviceIndex)` equality.

#### 5.2 InputActionMap Update Cycle

`InputActionMap::Update(dt, blockedSources)` (`InputAction.cpp:125-171`):

**Phase 1 -- Axis evaluation**:
```
For each axis binding:
  raw = ComputeRawAxis(entry)          [key pair, mouse delta, or gamepad axis]
  For each modifier in chain:
    raw = modifier->Apply(raw, dt)
  m_CachedAxisValues[name] = raw
```

**Phase 2 -- Action state evaluation**:
```
For each action (single-source and chord):
  state = EvaluateActionState(name, blockedSources)
  m_CachedActionStates[name] = state
```

`EvaluateActionState` (`InputAction.cpp:347-387`): iterates all single-source bindings
(OR), then all chord bindings (OR), skipping any source that appears in
`blockedSources`. Returns `{HasAvailableBinding, Down, Pressed, Released}`.

**Phase 3 -- Trigger advancement**:
```
For each trigger:
  state = m_CachedActionStates[name] (or evaluate if not cached)
  if no available binding: TriggerState::None
  else: trigger->Evaluate(state.Down, state.Pressed, state.Released, dt)
  m_TriggerStates[name] = result
```

Query methods (`IsActionDown`, `WasActionPressedThisFrame`, `GetAxis`,
`WasActionTriggeredThisFrame`) return cached values when available, falling back to live
evaluation otherwise.

---

### 6. Processing Pipeline

#### 6.1 Modifiers

`InputModifier.h` defines the base class and five built-in modifiers:

| Modifier | Apply Logic | Stateful? |
|----------|------------|-----------|
| `DeadZone(threshold)` | `abs(value) < threshold ? 0 : value` | No |
| `Sensitivity(scale)` | `value * scale` | No |
| `Clamp(min, max)` | `std::clamp(value, min, max)` | No |
| `Negate` | `-value` | No |
| `Smooth(halfLife)` | `smoothed += alpha * (value - smoothed)` where `alpha = 1 - 2^(-dt/halfLife)` | Yes (`mutable m_Smoothed`) |

Modifiers are applied in insertion order during `InputActionMap::Update()`. The `Apply`
method signature is `const` on the base class, but `Smooth` uses a `mutable` member to
maintain exponential moving average state.

#### 6.2 Triggers

`InputTrigger.h` defines the base class and five built-in triggers:

| Trigger | State Machine | Fires When |
|---------|--------------|------------|
| `PressedTrigger` | Stateless | `pressed == true` |
| `ReleasedTrigger` | Stateless | `released == true` |
| `HoldTrigger(duration)` | Accumulates `m_Elapsed` while held, fires once | `m_Elapsed >= duration` and not yet fired |
| `TapTrigger(maxDuration)` | Tracks press duration, fires on release | Released within `maxDuration` |
| `DoubleTapTrigger(maxGap)` | Tracks gap between two presses | Second press arrives within `maxGap` |

All stateful triggers implement `Reset()`, called when a context is deactivated via
`InputActionMap::ResetRuntimeState()`.

---

### 7. Context Routing

`InputContextStack` (`InputContextStack.h` / `InputContextStack.cpp`) manages a
priority-sorted vector of `InputContext` entries.

#### 7.1 Update Flow with Chord Blocking

`InputContextStack::Update(dt)` (`InputContextStack.cpp:55-70`):

```
blockedSources = {}
for context in sorted_by_priority_descending:
    if not active or no action map: skip
    context.ActionMap->Update(dt, blockedSources)
    context.ActionMap->AppendBlockingChordSources(blockedSources)
    if context.ConsumesInput: break
```

`AppendBlockingChordSources` (`InputAction.cpp:298-312`): iterates all chord bindings.
If any source in a chord is currently pressed (`IsChordBlocking`), all sources from that
chord are added to `blockedSources`. This prevents a lower-priority context from treating
a shared modifier key (e.g. Ctrl) as a standalone action when a higher-priority context
has an engaged chord (e.g. Ctrl+S).

#### 7.2 Cross-Context Queries

`IsActionDown(action)` (`InputContextStack.cpp:74-93`):

```
blockedSources = {}
for context in sorted_by_priority_descending:
    if not active: skip
    if context.ActionMap->HasActionAvailable(action, blockedSources):
        return context.ActionMap->IsActionDown(action, blockedSources)
    context.ActionMap->AppendBlockingChordSources(blockedSources)
    if context.ConsumesInput: break
return false
```

This means queries walk the stack top-down, accumulating blocked sources from higher
contexts, and return the result from the first reachable context that defines the
requested action.

`GetAxis` is simpler: no blocking logic is applied, just the first context that defines
the axis wins.

---

### 8. Advanced Patterns

**ChordBinding** (`InputPatterns.h:9-12`):

A `std::vector<InputSource>` where all sources must be simultaneously active. Integrated
directly into `InputActionMap` as a separate binding list (`m_ChordActions`) rather than
a standalone system.

**ComboTracker** (`InputPatterns.h:14-40`, `InputPatterns.cpp:1-41`):

Tracks progress through a `ComboBinding` sequence (ordered steps, each with a
`MaxDelay`).

```
Update(dt):
  if in progress: advance timer
    if timer > current step's MaxDelay: Reset() to step 0
  if current step's source was pressed this frame:
    advance to next step
    if all steps completed: Reset() and return true (combo fired)
  return false
```

Note: after a timeout Reset(), the same frame's input can immediately match step 0 of a
new attempt.

**DoubleTapTrigger**: lives in the standard trigger pipeline, not in a separate pattern
system. This means double-tap actions are bound and queried identically to other triggered
actions.

---

### 9. EventBus

`EventBus.h` implements a type-safe publish/subscribe bus.

**Type dispatch**: uses `std::type_index(typeid(T))` as key into an
`unordered_map<type_index, unique_ptr<ISubscriberList>>`. Each concrete `SubscriberList<T>`
holds a vector of `{Id, Handler, PendingRemoval}` entries.

**Subscribe** (`EventBus.h:29-47`):

1. Get or create `SubscriberList<T>`
2. Assign monotonic ID from `m_NextId++`
3. Push entry to list
4. Return `ScopedConnection` wrapping a lambda that marks the entry as `PendingRemoval`

**Publish** (`EventBus.h:53-77`):

1. Increment `DispatchDepth`
2. Iterate entries by index (not iterator), call handler if not pending removal
3. Decrement `DispatchDepth`
4. If depth reaches 0 and `Dirty` flag set: erase-remove all pending entries

The index-based iteration means subscribers added during dispatch may be visited in the
same pass (the `size()` check grows as new entries are appended).

**ScopedConnection** (`ScopedConnection.h`):

RAII wrapper holding a `unique_ptr<function<void()>>` to the unsubscribe lambda. Move-only.
`Disconnect()` can be called explicitly for early unsubscription.

---

### 10. Recording and Replay

#### 10.1 Recording

`InputRecorder` (`InputRecording.h:57-85`, `InputRecording.cpp:118-321`):

- Implements `InputDeviceManagerObserver::OnAfterPollAll`
- Attaches to an `InputDeviceManager` via `Attach(manager)` (registers as observer)
- When recording: after each poll, captures all device state into an `InputFrame`
- Capture functions query each device through the `InputDeviceManager` interface, not
  through GLFW directly

`CaptureFrame` collects:
- Keyboard: presence, connection, 512 key states (bool -> uint8)
- Mouse: presence, connection, 8 buttons, X/Y position, scroll delta
- Gamepads: for each gamepad device, index, connection, 15 buttons, 6 axes

#### 10.2 Replay

`InputReplaySession` (`InputRecording.h:87-132`, `InputRecording.cpp:323-436`):

- Implements `InputDeviceManagerObserver::OnBeforePollAll`
- Advances frame pointer **before** device polling, so replay devices see the current
  recorded frame when `Poll()` is called
- Maintains three frame pointers: `m_FrameBeforePrevious`, `m_PreviousFrame`,
  `m_CurrentFrame` -- this supports mouse delta derivation from three consecutive frames
- Tracks timing mismatch: if `abs(recorded_dt - runtime_dt) > 0.0001`, sets
  `m_HasTimingMismatch = true`

Replay devices (described in design doc but not yet present as separate source files) would
read from the session's current/previous frame pointers through the standard `InputDevice`
interface, making them transparent to all polling consumers.

#### 10.3 Binary Format

`InputRecorder::SaveToFile` / `LoadFromFile` (`InputRecording.cpp:168-293`):

```
Header:
  [4 bytes] Magic: 0x52504E49 ("INPR")
  [4 bytes] Version: 1
  [4 bytes] Frame count

Per frame:
  [8 bytes] Frame number (uint64)
  [4 bytes] Delta time (float)
  [1 byte]  Keyboard present
  [1 byte]  Keyboard connected
  [512 bytes] Key states (uint8 per key)
  [1 byte]  Mouse present
  [1 byte]  Mouse connected
  [8 bytes] Mouse buttons (uint8 per button)
  [4 bytes] Mouse X (float)
  [4 bytes] Mouse Y (float)
  [4 bytes] Mouse scroll delta (float)
  [4 bytes] Gamepad count (uint32)
  Per gamepad:
    [1 byte]  Device index
    [1 byte]  Connected
    [15 bytes] Buttons (uint8 per button)
    [24 bytes] Axes (float per axis)
```

Validation: on load, verifies magic, version, and that the entire byte stream is consumed
exactly (no trailing data).

---

### 11. Key Implementation Details

| Mechanism | Implementation Location | Key Point |
|-----------|------------------------|-----------|
| Polling/event split | `Input.cpp` vs `EventBus.h` | Two independent paths sharing only scroll and typed codes |
| Capture flags gate gameplay queries | `Input.cpp:94-101`, `InputSource.cpp:28-30` | `s_KeyboardCaptured` / `s_MouseCaptured` checked in both facade and source-level queries |
| Mouse position always readable | `Input.cpp:148-157` | `GetMousePosition()` does not check `s_MouseCaptured` |
| Device slot model | `InputDeviceManager.cpp:158-162` | `FindDeviceBySlot(Type, Index)` -- linear scan of device vector |
| First-frame gamepad guard | `GamepadDevice.cpp:46-47` | `m_HasPolledOnce` prevents false disconnect event on first poll |
| Scroll accumulator pattern | `MouseDevice.cpp:46-48` | Callback accumulates, Poll snapshots and resets |
| Chord blocking propagation | `InputContextStack.cpp:64-65` | After Update, `AppendBlockingChordSources` collects blocked keys for lower contexts |
| Cached action states | `InputAction.cpp:144-155` | `Update()` pre-evaluates all actions to align trigger and query semantics |
| Combo timeout allows same-frame restart | `InputPatterns.cpp:18-24` | After timeout Reset, step 0 is immediately checked |
| Observer copy in PollAll | `InputDeviceManager.cpp:104` | Guards against observer list mutation during callbacks |
| Deferred removal in EventBus | `EventBus.h:68-76` | Entries marked `PendingRemoval`, compacted only at `DispatchDepth == 0` |
| Binary recording validation | `InputRecording.cpp:288-289` | `offset != bytes.size()` check rejects truncated or padded files |

---

## Part II: Comparison with Modern Game Engines

### 12. Polling vs Event Model

| Dimension | RTRLab | Unreal Engine | Unity | Godot 4 |
|-----------|--------|---------------|-------|---------|
| Continuous input | Polling (`Input::IsKeyDown`) | Polling (`Enhanced Input` evaluates per-tick) | Polling (`InputAction.ReadValue<T>()`) | Mixed (`Input.is_action_pressed()`) |
| Discrete notifications | `EventBus` (separate system) | Delegates (`FInputActionBinding`) | Callbacks (`InputAction.performed`) | `_input(InputEvent)` virtual |
| Separation | Hard split: polling and events are separate types | Soft split: Enhanced Input is polling-centric but has callbacks | Unified: actions have both polling and callback interfaces | Unified: events and polling share the `InputEvent` type |
| Scroll handling | Dual-path (event + accumulator) | Event-based (`FInputAxisKeyMapping`) | Event-based (scroll is an axis) | Event-based (`InputEventMouseMotion`) |

**Analysis**: RTRLab's hard split between polling and events is the cleanest separation
of any engine surveyed. Unreal Enhanced Input converges toward the same model (polling for
continuous, delegates for discrete) but still carries legacy event-based bindings. Unity
and Godot unify both models behind a single abstraction, which is more convenient but makes
held-state queries less natural.

### 13. Action Mapping and Context Routing

| Dimension | RTRLab | Unreal Enhanced Input | Unity Input System | Godot 4 |
|-----------|--------|-----------------------|--------------------|---------|
| Action container | `InputActionMap` | `UInputMappingContext` | `InputActionMap` | `InputMap` (project settings) |
| Priority routing | `InputContextStack` (explicit stack) | Mapping context priority (per `AddMappingContext`) | Manual enable/disable of action maps | No priority system |
| Chord support | `ChordBinding` in action map | `UInputTriggerChordAction` | Composite binding with modifiers | No native chord |
| Combo support | `ComboTracker` (standalone) | Custom trigger subclass | Not built-in | Not built-in |
| Serialization | JSON via `InputActionSerialization.h` | UAsset (binary) | `.inputactions` (JSON) | `project.godot` (text) |
| String-keyed lookups | Yes (`unordered_map<string, ...>`) | No (UObject pointers) | Yes (action name strings) | Yes (StringName) |

**Analysis**: RTRLab's `InputContextStack` with chord blocking is the most explicit
priority-routing system of the four. Unreal's mapping context priority achieves the same
goal but the chord-blocking propagation is handled implicitly by the trigger system.
Unity requires manual map enable/disable and has no cross-map chord awareness. Godot has
no priority routing at all.

### 14. Device Abstraction

| Dimension | RTRLab | Unreal Engine | Unity | Godot 4 |
|-----------|--------|---------------|-------|---------|
| Device interface | `InputDevice` (flat virtual) | `IInputDevice` + `FInputDeviceScope` | `InputDevice` (layout-based) | `Input` singleton (no device abstraction) |
| Device discovery | Slot model `(Type, Index)` | Platform subsystem registration | Runtime auto-detection + layout matching | Automatic |
| Replay devices | Synthetic devices implementing `InputDevice` | `DemoNetDriver` (network-level replay) | Not built-in | Not built-in |
| Hot-swap | Replace device in slot via `AddDevice` | Runtime device change notification | Runtime layout matching | N/A |
| Force feedback | Not supported | `IForceFeedbackSystem` | `InputDevice.ExecuteCommand` | `Input.start_joy_vibration()` |
| Touch input | `Type::Touch` defined, not implemented | Full touch support | Full touch support | Full touch support |

**Analysis**: RTRLab's device abstraction is intentionally minimal. The slot model makes
replay and hot-swap predictable: a replay keyboard occupies the same slot as the live
keyboard, so all downstream queries work unchanged. This is simpler than Unity's
layout-based matching but less flexible for exotic devices.

### 15. Processing Pipeline (Modifiers and Triggers)

| Dimension | RTRLab | Unreal Enhanced Input | Unity Input System |
|-----------|--------|-----------------------|--------------------|
| Value transform | `InputModifier` chain | `UInputModifier` chain | `InputProcessor` chain |
| Timing control | `InputTrigger` state machine | `UInputTrigger` state machine | `IInputInteraction` |
| Built-in modifiers | DeadZone, Sensitivity, Clamp, Negate, Smooth | DeadZone, Negate, Scalar, Smooth, ResponseCurve, FOVScaling, SwizzleAxis, ToWorldSpace | StickDeadzone, AxisDeadzone, InvertProcessor, NormalizeProcessor, ScaleProcessor, ClampProcessor |
| Built-in triggers | Pressed, Released, Hold, Tap, DoubleTap | Pressed, Released, Hold, HoldAndRelease, Tap, Pulse, ChordAction, ComboAction | Press, Hold, Tap, SlowTap, MultiTap |
| Custom extensibility | Subclass `InputModifier` / `InputTrigger` | Subclass in C++ or Blueprint | Implement `IInputProcessor` / `IInputInteraction` |

**Analysis**: RTRLab's pipeline closely mirrors Unreal Enhanced Input's architecture. The
modifier/trigger split is the same conceptual model. RTRLab's built-in set is smaller but
covers the most common cases. The main gap is that Unreal provides spatial transforms
(ToWorldSpace, SwizzleAxis) and response curves, which would be needed for 3D camera
control with gamepads.

### 16. Recording and Replay

| Dimension | RTRLab | Unreal Engine | Unity | Godot 4 |
|-----------|--------|---------------|-------|---------|
| Built-in recording | Yes (polling-state snapshots) | `DemoNetDriver` (network-level) | No built-in input recording | No built-in |
| Scope | Polling state only (keyboard, mouse, gamepad) | Full game state (actor replication) | N/A | N/A |
| Determinism | Frame-aligned, dt-tracked | Tick-aligned, network-deterministic | N/A | N/A |
| Format | Custom binary (magic INPR, versioned) | `.replay` (proprietary) | N/A | N/A |
| Integration | Device-manager observer hooks | Deeply integrated with networking | N/A | N/A |

**Analysis**: RTRLab's recording/replay system fills a niche that neither Unreal, Unity,
nor Godot covers out of the box: lightweight, polling-state-only input capture for
deterministic gameplay reproduction. Unreal's replay system is far more comprehensive but
also far more complex (it serializes the entire game state via the networking layer).
RTRLab's observer-based integration is elegant: recording and replay hook into the device
manager's poll boundaries without modifying `Input` or `Application`.

### 17. Mechanisms Present in Mature Engines but Absent in RTRLab

| Missing Mechanism | Unreal / Unity Approach | Impact Assessment for RTRLab |
|-------------------|-------------------------|------------------------------|
| Force feedback / haptics | Unreal `SetForceFeedbackValues`, Unity `Gamepad.SetMotorSpeeds` | Needed for gamepad-heavy gameplay demos |
| Touch input | Full touch device support in all three engines | `InputDevice::Type::Touch` is defined but unimplemented |
| UI event propagation | Unreal `FReply` / Slate routing, Unity `EventSystem` | Currently ImGui capture flags; a full UI routing system would need event propagation |
| Runtime rebinding UI | Unreal `UInputKeySelector`, Unity `RebindingOperation` | `InputActionSerialization` enables persistence but no UI exists for runtime rebinding |
| Input buffering | Common in fighting/action games; usually custom | Useful for frame-perfect action games |
| Composite 2D/3D actions | Unreal `FInputActionValue` supports `Axis2D`/`Axis3D`, Unity `Vector2`/`Vector3` composites | RTRLab actions are 1D only; 2D camera look requires two separate axes |
| Per-platform default bindings | Unreal `UInputMappingContext` per platform, Unity `InputActionAsset` per scheme | Single binding set currently; no platform-specific defaults |

### 18. Overall Positioning

RTRLab's input system makes architecturally mature choices:

- **Polling/event split** -- cleaner than Unreal's legacy layer and Godot's mixed model.
- **Action mapping with modifier/trigger pipeline** -- directly aligned with Unreal
  Enhanced Input's proven architecture.
- **Explicit context routing with chord blocking** -- more predictable than any surveyed
  engine's priority system.
- **Observer-based recording/replay** -- uniquely clean integration that no surveyed engine
  matches for this use case.

The system is at the **"complete core with room for extension"** stage:

```
Godot 4 InputMap  --  RTRLab current  ---------  Unreal Enhanced Input
     |                    |                            |
  Flat action map       Full action mapping            Full action mapping
  No priority routing   Priority stack + chord block   Priority + trigger-based blocking
  No modifier pipeline  5 modifiers, 5 triggers        15+ modifiers, 10+ triggers
  No recording          Polling-state replay           Network-level replay
  No device abstraction Device slot model              Full device subsystem
```

The most impactful extensions would be: composite 2D actions for camera control, force
feedback for gamepad demos, and a runtime rebinding interface to complement the existing
serialization support.
