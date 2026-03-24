# Input System & Event System — Design Document

Updated 2026-03-14. Describes the **complete end-state** architecture for these two systems,
from the minimal foundation through to production game-engine features.
Then defines a **phased implementation plan** aligned with the current project stage.

> **Design Philosophy**: Polling-first for continuous input, event-driven for discrete
> notifications. Input Action Mapping decouples game logic from physical keys.
> Signal/Slot (publish-subscribe) replaces inheritance-based event dispatch.
> Modular layers — each layer is independently testable and can be implemented incrementally.

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Layer 0 — Key & Mouse Codes](#2-layer-0--key--mouse-codes)
3. [Layer 1 — Input Device State (Polling)](#3-layer-1--input-device-state-polling)
4. [Layer 2 — Input Action Map](#4-layer-2--input-action-map)
5. [Layer 3 — Input Modifiers & Triggers](#5-layer-3--input-modifiers--triggers)
6. [Layer 4 — Input Context Stack](#6-layer-4--input-context-stack)
7. [Layer 5 — Input Device Abstraction](#7-layer-5--input-device-abstraction)
8. [Layer 6 — Event Bus (Signal/Slot)](#8-layer-6--event-bus-signalslot)
9. [Layer 7 — Window Event Production](#9-layer-7--window-event-production)
10. [Layer 8 — ImGui Input Routing](#10-layer-8--imgui-input-routing)
11. [Layer 9 — Chord, Combo & Gesture Recognition](#11-layer-9--chord-combo--gesture-recognition)
12. [Layer 10 — Input Recording & Replay](#12-layer-10--input-recording--replay)
13. [Data Flow Diagram](#13-data-flow-diagram)
14. [File Layout](#14-file-layout)
15. [Phased Implementation Plan](#15-phased-implementation-plan)
16. [Appendix A: Comparison with Cherno-Style](#16-appendix-a-comparison-with-cherno-style)
17. [Appendix B: Reference — How Production Engines Do It](#17-appendix-b-reference--how-production-engines-do-it)

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

```
Layer 10  Input Recording & Replay         (injectable input source)
Layer 9   Chord / Combo / Gesture          (state machines on top of actions)
Layer 8   ImGui Input Routing              (capture flags block polling)
Layer 7   Window Event Production          (GLFW callbacks → EventBus)
Layer 6   Event Bus (Signal/Slot)          (type-safe pub/sub, RAII connections)
Layer 5   Input Device Abstraction         (keyboard, mouse, gamepad as devices)
Layer 4   Input Context Stack              (priority-based action map activation)
Layer 3   Input Modifiers & Triggers       (dead zone, sensitivity, hold/tap)
Layer 2   Input Action Map                 (logical actions ↔ physical bindings)
Layer 1   Input Device State (Polling)     (double-buffered per-frame state)
Layer 0   Key & Mouse Codes                (typed constants, no GLFW dependency)
```

Layers 0-2 and Layer 6 form the **core** — they are needed for a functional system.
Layers 3-5 and 7-10 are **extensions** that bring the system to production-engine parity.
Each layer only depends on layers below it.

---

## 2. Layer 0 — Key & Mouse Codes

**Goal**: Replace raw `int` key codes with typed enums. Remove GLFW header dependency
from all code outside `Input.cpp` and `Window.cpp`.

### KeyCode.h

```cpp
#pragma once

#include <cstdint>

// Values match GLFW constants so no conversion is needed at the GLFW boundary.
// This header does NOT include <GLFW/glfw3.h>.

namespace Key
{
    using Code = uint16_t;

    enum : Code
    {
        // Printable keys
        Space           = 32,
        Apostrophe      = 39,   // '
        Comma           = 44,   // ,
        Minus           = 45,   // -
        Period          = 46,   // .
        Slash           = 47,   // /

        D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,

        Semicolon       = 59,   // ;
        Equal           = 61,   // =

        A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        LeftBracket     = 91,   // [
        Backslash       = 92,
        RightBracket    = 93,   // ]
        GraveAccent     = 96,   // `

        // Function keys
        Escape          = 256,
        Enter           = 257,
        Tab             = 258,
        Backspace       = 259,
        Insert          = 260,
        Delete          = 261,
        Right           = 262,
        Left            = 263,
        Down            = 264,
        Up              = 265,
        PageUp          = 266,
        PageDown        = 267,
        Home            = 268,
        End             = 269,
        CapsLock        = 280,
        ScrollLock      = 281,
        NumLock         = 282,
        PrintScreen     = 283,
        Pause           = 284,

        F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // Modifier keys
        LeftShift       = 340,
        LeftControl     = 341,
        LeftAlt         = 342,
        LeftSuper       = 343,
        RightShift      = 344,
        RightControl    = 345,
        RightAlt        = 346,
        RightSuper      = 347,
        Menu            = 348
    };
}
```

### MouseCode.h

```cpp
#pragma once

#include <cstdint>

namespace Mouse
{
    using Code = uint16_t;

    enum : Code
    {
        Button0 = 0, Button1, Button2, Button3, Button4, Button5, Button6, Button7,

        Left    = Button0,
        Right   = Button1,
        Middle  = Button2
    };
}
```

**Design decisions**:

- `using Code = uint16_t` instead of `enum class` — allows implicit conversion to
  `int` for GLFW calls without `static_cast` noise, while still providing named constants.
- Values are GLFW-identical so the translation layer is zero-cost.
- Namespace (`Key::`, `Mouse::`) instead of class scope — shorter call sites
  (`Key::W` vs `KeyCode::Key::W`).

---

## 3. Layer 1 — Input Device State (Polling)

**Goal**: Per-frame polling of keyboard, mouse position, mouse delta, and mouse
buttons. Additionally, track **edge detection** (pressed-this-frame / released-this-frame)
via double-buffered state.

### Input.h (revised)

```cpp
#pragma once

#include <utility>
#include "KeyCode.h"
#include "MouseCode.h"

struct GLFWwindow;

class Input
{
public:
    // --- Lifecycle (called by Application, not by user code) ---
    static void Initialize(GLFWwindow* window);

    // Must be called ONCE per frame, BEFORE any layer's OnUpdate().
    // Snapshots current GLFW state into the "current" buffer and
    // moves the old "current" into "previous".
    static void BeginFrame();

    // --- Keyboard ---
    static bool IsKeyDown(Key::Code key);               // held right now
    static bool WasKeyPressedThisFrame(Key::Code key);   // down this frame, up last frame
    static bool WasKeyReleasedThisFrame(Key::Code key);  // up this frame, down last frame

    // --- Mouse buttons ---
    static bool IsMouseButtonDown(Mouse::Code button);
    static bool WasMouseButtonPressedThisFrame(Mouse::Code button);
    static bool WasMouseButtonReleasedThisFrame(Mouse::Code button);

    // --- Mouse position & delta ---
    static std::pair<float, float> GetMousePosition();
    static std::pair<float, float> GetMouseDelta();
    static float GetMouseX();
    static float GetMouseY();

    // --- Mouse scroll (accumulated this frame) ---
    static float GetScrollDelta();

    // --- Capture flags (set by ImGui layer, Layer 8) ---
    static void SetKeyboardCaptured(bool captured);
    static void SetMouseCaptured(bool captured);
    static bool IsKeyboardCaptured();
    static bool IsMouseCaptured();

    // --- Scroll accumulator (fed by GLFW scroll callback) ---
    static void AccumulateScroll(float yOffset);

private:
    static GLFWwindow* s_Window;

    // Double-buffered keyboard state.
    // Index by GLFW key code. GLFW_KEY_LAST = 348, so 512 is safe.
    static constexpr int KEY_STATE_SIZE = 512;
    static bool s_CurrentKeys[KEY_STATE_SIZE];
    static bool s_PreviousKeys[KEY_STATE_SIZE];

    // Double-buffered mouse button state.
    static constexpr int MOUSE_BUTTON_COUNT = 8;
    static bool s_CurrentMouseButtons[MOUSE_BUTTON_COUNT];
    static bool s_PreviousMouseButtons[MOUSE_BUTTON_COUNT];

    // Mouse position tracking.
    static float s_MouseX, s_MouseY;
    static float s_LastMouseX, s_LastMouseY;
    static bool  s_FirstMouseSample;

    // Scroll accumulator (set via GLFW scroll callback, consumed per frame).
    static float s_ScrollAccumulator;
    static float s_ScrollThisFrame;

    // Capture flags.
    static bool s_KeyboardCaptured;
    static bool s_MouseCaptured;
};
```

### Input.cpp — BeginFrame sketch

```cpp
void Input::BeginFrame()
{
    // Swap key state buffers
    std::memcpy(s_PreviousKeys, s_CurrentKeys, sizeof(s_CurrentKeys));
    for (int i = 0; i < KEY_STATE_SIZE; ++i)
        s_CurrentKeys[i] = (glfwGetKey(s_Window, i) == GLFW_PRESS);

    // Swap mouse button buffers
    std::memcpy(s_PreviousMouseButtons, s_CurrentMouseButtons, sizeof(s_CurrentMouseButtons));
    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
        s_CurrentMouseButtons[i] = (glfwGetMouseButton(s_Window, i) == GLFW_PRESS);

    // Mouse position & delta
    double mx, my;
    glfwGetCursorPos(s_Window, &mx, &my);
    s_LastMouseX = s_MouseX;
    s_LastMouseY = s_MouseY;
    s_MouseX = static_cast<float>(mx);
    s_MouseY = static_cast<float>(my);

    if (s_FirstMouseSample)
    {
        s_LastMouseX = s_MouseX;
        s_LastMouseY = s_MouseY;
        s_FirstMouseSample = false;
    }

    // Consume scroll accumulator
    s_ScrollThisFrame = s_ScrollAccumulator;
    s_ScrollAccumulator = 0.0f;
}

bool Input::IsKeyDown(Key::Code key)
{
    if (s_KeyboardCaptured) return false;
    return key < KEY_STATE_SIZE && s_CurrentKeys[key];
}

bool Input::WasKeyPressedThisFrame(Key::Code key)
{
    if (s_KeyboardCaptured) return false;
    return key < KEY_STATE_SIZE && s_CurrentKeys[key] && !s_PreviousKeys[key];
}

bool Input::WasKeyReleasedThisFrame(Key::Code key)
{
    if (s_KeyboardCaptured) return false;
    return key < KEY_STATE_SIZE && !s_CurrentKeys[key] && s_PreviousKeys[key];
}
```

**Why double-buffered polling instead of GLFW key callbacks?**

- Polling is deterministic — state is sampled at a well-defined point each frame.
- No risk of missing events between frames or processing stale callback data.
- Edge detection (`WasKeyPressedThisFrame`) gives you the "event-like" one-shot
  behavior without an event object.
- Scroll is the one exception: GLFW only reports scroll via callback, so we
  accumulate it and expose per-frame.

**Integration point** — `Application::Run()` calls `Input::BeginFrame()` once
at the top of the frame loop, before iterating layers.

---

## 4. Layer 2 — Input Action Map

**Goal**: Decouple game/demo logic from physical key bindings. Provide two
abstractions: **Actions** (discrete, boolean) and **Axes** (continuous, float).

### InputAction.h

```cpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "KeyCode.h"
#include "MouseCode.h"

// An InputSource can be a key, mouse button, or (future) gamepad button / axis.
struct InputSource
{
    enum class Type : uint8_t { Key, MouseButton, GamepadButton, GamepadAxis };

    Type        SourceType;
    uint16_t    Code;       // Key::Code, Mouse::Code, or device-specific code
    uint8_t     DeviceIndex = 0; // Which gamepad (0-3), ignored for keyboard/mouse

    static InputSource FromKey(Key::Code key)
    {
        return { Type::Key, key, 0 };
    }

    static InputSource FromMouseButton(Mouse::Code button)
    {
        return { Type::MouseButton, button, 0 };
    }
};

// Binding for a 1D axis: positive source and negative source.
// Value is +1 when positive is held, -1 when negative is held, 0 when neither/both.
struct AxisBinding
{
    InputSource Positive;
    InputSource Negative;
};

class InputActionMap
{
public:
    // --- Registration (typically in OnAttach or setup code) ---

    // Bind an action name to one or more input sources.
    // Multiple calls with the same name ADD bindings (OR logic).
    void BindAction(const std::string& name, InputSource source);

    // Convenience overload for key-only actions.
    void BindAction(const std::string& name, Key::Code key);

    // Bind a 1D axis to a positive/negative key pair.
    void BindAxis(const std::string& name, Key::Code positive, Key::Code negative);

    // Bind a 1D axis to mouse delta (X or Y).
    enum class MouseAxis : uint8_t { X, Y, ScrollY };
    void BindAxis(const std::string& name, MouseAxis mouseAxis);

    // Remove all bindings for a given action or axis name.
    void Unbind(const std::string& name);

    // --- Queries (called per frame in OnUpdate) ---

    // True if ANY bound source is currently held.
    bool IsActionDown(const std::string& name) const;

    // True only on the frame the action transitions from up to down.
    bool WasActionPressedThisFrame(const std::string& name) const;

    // True only on the frame the action transitions from down to up.
    bool WasActionReleasedThisFrame(const std::string& name) const;

    // Returns a float in [-1, +1] for key axes, or raw delta for mouse axes.
    float GetAxis(const std::string& name) const;

private:
    // Action name -> list of sources (OR: any one triggers the action)
    std::unordered_map<std::string, std::vector<InputSource>> m_Actions;

    // Axis name -> binding
    struct AxisEntry
    {
        enum class Kind { KeyPair, MouseAxis };
        Kind kind;
        AxisBinding     keyPair;    // used when kind == KeyPair
        MouseAxis       mouseAxis;  // used when kind == MouseAxis
    };
    std::unordered_map<std::string, AxisEntry> m_Axes;
};
```

### Usage example — ShadowMapping demo

```cpp
void ShadowMapping::OnAttach()
{
    // ... existing setup ...

    // Define input bindings for this demo
    m_InputMap.BindAxis("MoveForward",  Key::W, Key::S);
    m_InputMap.BindAxis("MoveRight",    Key::D, Key::A);
    m_InputMap.BindAxis("MoveUp",       Key::E, Key::Q);
    m_InputMap.BindAxis("LookX",        InputActionMap::MouseAxis::X);
    m_InputMap.BindAxis("LookY",        InputActionMap::MouseAxis::Y);

    m_InputMap.BindAction("ShowFinalColor",  Key::D1);
    m_InputMap.BindAction("ShowShadowMap",   Key::D2);
    m_InputMap.BindAction("ToggleLookMode",  Mouse::Right);
}

void ShadowMapping::OnUpdate(double dt)
{
    // Continuous movement — polling via action map
    float forward = m_InputMap.GetAxis("MoveForward");
    float right   = m_InputMap.GetAxis("MoveRight");
    float up      = m_InputMap.GetAxis("MoveUp");

    if (forward != 0.0f) m_CameraController.Move(m_Camera.GetFront() * forward, dt);
    if (right   != 0.0f) m_CameraController.Move(m_Camera.GetRight() * right, dt);
    if (up      != 0.0f) m_CameraController.Move(glm::vec3(0,1,0) * up, dt);

    // Discrete actions — edge detection, no event system needed
    if (m_InputMap.WasActionPressedThisFrame("ShowFinalColor"))
        m_OutputMode = SceneRendererOutput::FinalColor;

    if (m_InputMap.WasActionPressedThisFrame("ShowShadowMap"))
        m_OutputMode = SceneRendererOutput::ShadowMap;

    // Mouse look
    if (m_InputMap.IsActionDown("ToggleLookMode"))
    {
        float lookX = m_InputMap.GetAxis("LookX");
        float lookY = m_InputMap.GetAxis("LookY");
        m_CameraController.OnMouseDelta(lookX, -lookY);
    }
}
```

**Design decisions**:

- `InputActionMap` is an **instance** owned by each demo/layer, not a global singleton.
  Different demos can have completely different bindings.
- String-keyed for readability and flexibility. For a rendering lab, the overhead of
  `unordered_map` lookup is negligible compared to draw calls.
- No config file for now. Bindings are registered in code. Config-file-driven
  rebinding is a future extension (see Layer 2 extensions below).
- Axis returns `float` in [-1, +1] for key pairs, or raw pixel delta for mouse.
  The consumer decides how to scale it (sensitivity, dt, etc.) — unless Layer 3
  modifiers are applied.

**Future extensions to this layer**:

- **Serialization**: save/load bindings to JSON for runtime rebinding UI.
- **Hashed action names**: replace `std::string` keys with compile-time hashed
  integers (`constexpr uint32_t hash("MoveForward")`) for zero-overhead lookup
  in hot paths.

**Known issue / design gap identified during review**:

- The current `InputActionMap` API is still fundamentally modeled as
  `bool` actions plus `float` axes. While Layer 5 introduces `InputValue { X, Y }`,
  the action layer itself does not yet expose a first-class action value type
  (for example: bool / 1D / 2D / 3D). If the long-term goal is to move closer
  to modern engine input systems, this may require a later API redesign when
  supporting gamepad sticks, composite movement vectors, or richer action data.

---

## 5. Layer 3 — Input Modifiers & Triggers

**Goal**: Process raw input values through a configurable pipeline before they reach
game logic. Inspired by Unreal Engine's Enhanced Input system.

### Problem this solves

Without this layer, every consumer repeats boilerplate:

```cpp
// Scattered across every demo/controller:
float lookX = rawMouseDeltaX * m_Sensitivity;
if (std::abs(lookX) < 0.01f) lookX = 0.0f;  // dead zone
lookX = glm::clamp(lookX, -maxTurnSpeed, maxTurnSpeed);
```

With modifiers, the action map handles it:

```cpp
m_InputMap.BindAxis("LookX", InputActionMap::MouseAxis::X);
m_InputMap.AddModifier("LookX", DeadZone{ 0.01f });
m_InputMap.AddModifier("LookX", Sensitivity{ 0.12f });
m_InputMap.AddModifier("LookX", Clamp{ -10.0f, 10.0f });
// GetAxis("LookX") now returns the fully processed value.
```

### InputModifier — value transformers

A modifier transforms a `float` input value. They are chained in order.

```cpp
#pragma once

#include <memory>

// Base class for input value modifiers.
// Applied in order: Raw → Modifier1 → Modifier2 → ... → Final value.
class InputModifier
{
public:
    virtual ~InputModifier() = default;

    // Transform the input value. deltaTime provided for time-dependent modifiers.
    virtual float Apply(float value, float deltaTime) const = 0;
};

// ---- Built-in modifiers ----

// Zeroes out values below a threshold (radial or per-axis).
class DeadZone : public InputModifier
{
public:
    explicit DeadZone(float threshold) : m_Threshold(threshold) {}

    float Apply(float value, float /*dt*/) const override
    {
        return std::abs(value) < m_Threshold ? 0.0f : value;
    }

private:
    float m_Threshold;
};

// Multiplies value by a scalar.
class Sensitivity : public InputModifier
{
public:
    explicit Sensitivity(float scale) : m_Scale(scale) {}

    float Apply(float value, float /*dt*/) const override
    {
        return value * m_Scale;
    }

private:
    float m_Scale;
};

// Clamps value to [min, max].
class Clamp : public InputModifier
{
public:
    Clamp(float min, float max) : m_Min(min), m_Max(max) {}

    float Apply(float value, float /*dt*/) const override
    {
        return std::clamp(value, m_Min, m_Max);
    }

private:
    float m_Min, m_Max;
};

// Inverts the axis (multiply by -1).
class Negate : public InputModifier
{
public:
    float Apply(float value, float /*dt*/) const override
    {
        return -value;
    }
};

// Smooths value over time using exponential moving average.
// Useful for reducing jitter on mouse or gamepad stick input.
class Smooth : public InputModifier
{
public:
    explicit Smooth(float halfLife) : m_HalfLife(halfLife) {}

    float Apply(float value, float dt) const override
    {
        // Exponential decay: smoothed = lerp(smoothed, value, 1 - 2^(-dt/halfLife))
        float alpha = 1.0f - std::pow(2.0f, -dt / m_HalfLife);
        m_Smoothed = m_Smoothed + alpha * (value - m_Smoothed);
        return m_Smoothed;
    }

private:
    float m_HalfLife;
    mutable float m_Smoothed = 0.0f; // mutable because Apply is const
};
```

### InputTrigger — when an action fires

A trigger controls **when** an action transitions to "triggered" state.
Without triggers, an action fires the instant its source is pressed.
Triggers add conditions like "hold for 0.5s" or "tap and release within 0.2s".

```cpp
#pragma once

// Result of evaluating a trigger this frame.
enum class TriggerState : uint8_t
{
    None,       // Trigger condition not met
    Ongoing,    // In progress (e.g., holding but haven't reached threshold)
    Triggered   // Condition met this frame
};

// Base class for input triggers.
class InputTrigger
{
public:
    virtual ~InputTrigger() = default;

    // Evaluate the trigger given the current action state.
    // `down`     = is the physical source currently held?
    // `pressed`  = was it just pressed this frame?
    // `released` = was it just released this frame?
    // `dt`       = frame delta time
    virtual TriggerState Evaluate(bool down, bool pressed, bool released, float dt) = 0;

    // Reset internal state (called when context is deactivated).
    virtual void Reset() {}
};

// ---- Built-in triggers ----

// Default: triggers on press.
class PressedTrigger : public InputTrigger
{
public:
    TriggerState Evaluate(bool /*down*/, bool pressed, bool /*released*/, float /*dt*/) override
    {
        return pressed ? TriggerState::Triggered : TriggerState::None;
    }
};

// Triggers after the source has been held for `duration` seconds.
class HoldTrigger : public InputTrigger
{
public:
    explicit HoldTrigger(float duration) : m_Duration(duration) {}

    TriggerState Evaluate(bool down, bool /*pressed*/, bool /*released*/, float dt) override
    {
        if (!down)
        {
            m_Elapsed = 0.0f;
            m_Fired = false;
            return TriggerState::None;
        }

        m_Elapsed += dt;

        if (!m_Fired && m_Elapsed >= m_Duration)
        {
            m_Fired = true;
            return TriggerState::Triggered;
        }

        return m_Fired ? TriggerState::None : TriggerState::Ongoing;
    }

    void Reset() override { m_Elapsed = 0.0f; m_Fired = false; }

private:
    float m_Duration;
    float m_Elapsed = 0.0f;
    bool  m_Fired = false;
};

// Triggers when the source is pressed and released within `maxDuration` seconds.
class TapTrigger : public InputTrigger
{
public:
    explicit TapTrigger(float maxDuration = 0.2f) : m_MaxDuration(maxDuration) {}

    TriggerState Evaluate(bool down, bool pressed, bool released, float dt) override
    {
        if (pressed)
        {
            m_Elapsed = 0.0f;
            m_Active = true;
            return TriggerState::Ongoing;
        }

        if (m_Active)
        {
            m_Elapsed += dt;

            if (released && m_Elapsed <= m_MaxDuration)
            {
                m_Active = false;
                return TriggerState::Triggered;
            }

            if (released || m_Elapsed > m_MaxDuration)
            {
                m_Active = false;
                return TriggerState::None;
            }

            return TriggerState::Ongoing;
        }

        return TriggerState::None;
    }

    void Reset() override { m_Elapsed = 0.0f; m_Active = false; }

private:
    float m_MaxDuration;
    float m_Elapsed = 0.0f;
    bool  m_Active = false;
};

// Triggers on release.
class ReleasedTrigger : public InputTrigger
{
public:
    TriggerState Evaluate(bool /*down*/, bool /*pressed*/, bool released, float /*dt*/) override
    {
        return released ? TriggerState::Triggered : TriggerState::None;
    }
};
```

### Integration with InputActionMap

The action map gains two new methods:

```cpp
class InputActionMap
{
public:
    // ... existing BindAction / BindAxis ...

    // Add a modifier to an axis. Modifiers are applied in insertion order.
    void AddModifier(const std::string& axisName, std::unique_ptr<InputModifier> modifier);

    // Set the trigger for an action. Default is PressedTrigger.
    void SetTrigger(const std::string& actionName, std::unique_ptr<InputTrigger> trigger);

    // Per-frame update: advances trigger state machines.
    // Must be called once per frame (typically by InputContextStack or Application).
    void Update(float dt);

    // Queries now return trigger-aware results:
    // WasActionTriggeredThisFrame checks TriggerState::Triggered.
    bool WasActionTriggeredThisFrame(const std::string& name) const;
    TriggerState GetActionTriggerState(const std::string& name) const;
};
```

**Design decisions**:

- Modifiers are **polymorphic** (`InputModifier*`) rather than `std::variant` because
  users will want to write custom modifiers. The virtual call overhead is negligible
  (a few modifiers per axis, once per frame).
- Triggers are **stateful** — they track time, so they must be `Update()`-ed each frame.
- `Smooth` uses `mutable` for its internal state because `Apply` is semantically
  a pure transformation from the caller's perspective.

---

## 6. Layer 4 — Input Context Stack

**Goal**: Allow multiple `InputActionMap` instances to coexist with well-defined
priority. Higher-priority contexts can block lower ones.

### Problem this solves

Without a context stack:

```cpp
void PauseMenu::OnUpdate(double dt) {
    if (m_InputMap.WasActionPressedThisFrame("Resume"))  // Escape
        Resume();
}

void Gameplay::OnUpdate(double dt) {
    if (m_InputMap.WasActionPressedThisFrame("OpenMenu"))  // Also Escape!
        OpenPauseMenu();
    // Bug: both fire on the same frame because there's no priority system
}
```

With a context stack:

```cpp
// In Application or InputSystem setup:
InputContextStack contextStack;

// Gameplay context at low priority (always active)
contextStack.Push("Gameplay", &gameplayMap, 0);

// When pause menu opens:
contextStack.Push("PauseMenu", &pauseMenuMap, 100, /*consumesInput=*/ true);
// Now "Gameplay" context's Escape binding is blocked.

// When pause menu closes:
contextStack.Pop("PauseMenu");
// "Gameplay" context resumes normally.
```

### InputContextStack.h

```cpp
#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include "InputAction.h"

struct InputContext
{
    std::string         Name;
    InputActionMap*     ActionMap;       // Non-owning. Lifetime managed by the layer/demo.
    int                 Priority;        // Higher = processed first.
    bool                ConsumesInput;   // If true, lower-priority contexts don't see input.
    bool                Active = true;   // Can be temporarily disabled without removing.
};

class InputContextStack
{
public:
    // Add a context. If a context with the same name exists, it is replaced.
    void Push(const std::string& name, InputActionMap* map,
              int priority = 0, bool consumesInput = false);

    // Remove a context by name.
    void Pop(const std::string& name);

    // Enable/disable a context without removing it.
    void SetActive(const std::string& name, bool active);

    // Per-frame update: iterates contexts from highest to lowest priority,
    // calls Update(dt) on each active context's ActionMap.
    // If a context has ConsumesInput=true AND any of its actions are active,
    // lower-priority contexts are skipped.
    void Update(float dt);

    // Query an action across all active contexts (highest priority wins).
    bool IsActionDown(const std::string& action) const;
    bool WasActionTriggeredThisFrame(const std::string& action) const;
    float GetAxis(const std::string& axis) const;

private:
    std::vector<InputContext> m_Contexts; // Sorted by priority descending.

    void SortByPriority();
};
```

### How it works at runtime

```
Frame N:
  InputContextStack::Update(dt)
    ├── [Priority 100] PauseMenu (active, consumesInput=true)
    │   ├── Update PauseMenu's ActionMap
    │   └── PauseMenu has active bindings → BLOCK lower contexts
    └── [Priority 0] Gameplay (BLOCKED, skipped)

Frame N+1 (after Resume):
  InputContextStack::Update(dt)
    └── [Priority 0] Gameplay (active, consumesInput=false)
        └── Update Gameplay's ActionMap → all bindings work normally
```

**Real engine parallels**:

| Engine | Equivalent Concept |
|--------|--------------------|
| Unreal | `UInputMappingContext` + priority parameter in `AddMappingContext()` |
| Unity  | `InputActionMap.Enable()` / `Disable()` + `PlayerInput` action map stack |
| Godot  | `InputMap` + `set_process_input()` enable/disable |

**Design decisions**:

- **Non-owning pointers** to ActionMaps. The context stack is a coordinator,
  not an owner. Each layer/demo owns its own ActionMap and can destroy it
  independently.
- **`ConsumesInput`** is a simple boolean, not per-action. This matches the
  common case: a modal UI either blocks all game input or it doesn't.
  Per-action blocking (e.g., "pause menu blocks WASD but not screenshot key")
  can be added later with an exclusion list.

---

## 7. Layer 5 — Input Device Abstraction

**Goal**: Abstract away the source of input so the system can support keyboard,
mouse, gamepad, and future devices without changing the action map or game logic.

### Problem this solves

Currently, `Input` is hardcoded to GLFW:

```cpp
bool Input::IsKeyDown(Key::Code key)
{
    return glfwGetKey(s_Window, key) == GLFW_PRESS;
}
```

Adding gamepad support means adding more GLFW-specific code to `Input.cpp`.
Adding a replay system means monkey-patching `Input`. The abstraction fixes both.

### InputDevice.h

```cpp
#pragma once

#include <string>
#include <cstdint>

// Unified input value. All devices report their state through this.
struct InputValue
{
    float X = 0.0f;     // Primary axis (or 1.0/0.0 for buttons)
    float Y = 0.0f;     // Secondary axis (for 2D inputs like sticks)

    bool AsBool() const { return X > 0.5f; }
    float AsFloat() const { return X; }
};

// Abstract input device. Each physical device type implements this.
class InputDevice
{
public:
    virtual ~InputDevice() = default;

    // Human-readable name (e.g., "Keyboard", "Mouse", "Gamepad 0").
    virtual const std::string& GetName() const = 0;

    // Device type identifier.
    enum class Type : uint8_t { Keyboard, Mouse, Gamepad, Touch, Custom };
    virtual Type GetType() const = 0;

    // Called once per frame to refresh device state.
    virtual void Poll() = 0;

    // Query a specific input on this device.
    // `code` is device-specific (Key::Code for keyboard, Mouse::Code for mouse, etc.)
    virtual InputValue GetInput(uint16_t code) const = 0;

    // For devices with continuous axes (mouse delta, gamepad stick):
    virtual InputValue GetAxis(uint16_t axisId) const { return {}; }

    // Is this device currently connected?
    virtual bool IsConnected() const { return true; }
};
```

### Concrete implementations (sketches)

```cpp
// KeyboardDevice — wraps GLFW keyboard polling
class KeyboardDevice : public InputDevice
{
public:
    explicit KeyboardDevice(GLFWwindow* window);

    const std::string& GetName() const override { return m_Name; }
    Type GetType() const override { return Type::Keyboard; }

    void Poll() override;   // Snapshots all key states (like current BeginFrame)
    InputValue GetInput(uint16_t code) const override;

private:
    GLFWwindow* m_Window;
    std::string m_Name = "Keyboard";
    bool m_Keys[512] = {};
    bool m_PrevKeys[512] = {};
};

// MouseDevice — wraps GLFW mouse polling
class MouseDevice : public InputDevice
{
public:
    explicit MouseDevice(GLFWwindow* window);

    Type GetType() const override { return Type::Mouse; }
    void Poll() override;
    InputValue GetInput(uint16_t code) const override;   // Mouse buttons
    InputValue GetAxis(uint16_t axisId) const override;  // 0=deltaX, 1=deltaY, 2=scrollY

private:
    GLFWwindow* m_Window;
    // ... position, delta, scroll state ...
};

// GamepadDevice — wraps GLFW joystick API
class GamepadDevice : public InputDevice
{
public:
    explicit GamepadDevice(int joystickId);

    Type GetType() const override { return Type::Gamepad; }
    void Poll() override;       // glfwGetGamepadState()
    InputValue GetInput(uint16_t code) const override;   // Buttons
    InputValue GetAxis(uint16_t axisId) const override;  // Stick axes, triggers

    bool IsConnected() const override;

private:
    int m_JoystickId;
    GLFWgamepadstate m_State = {};
    GLFWgamepadstate m_PrevState = {};
};
```

### InputDeviceManager

```cpp
class InputDeviceManager
{
public:
    void AddDevice(std::unique_ptr<InputDevice> device);
    void RemoveDevice(const std::string& name);

    // Poll all devices. Called once per frame by Application.
    void PollAll();

    // Find a device by type.
    InputDevice* GetDevice(InputDevice::Type type, uint8_t index = 0) const;

    // Iterate all devices.
    const std::vector<std::unique_ptr<InputDevice>>& GetDevices() const;

    // Hot-plug detection (for gamepads).
    void CheckConnections();

private:
    std::vector<std::unique_ptr<InputDevice>> m_Devices;
};
```

### How InputActionMap uses devices

With the device abstraction, `InputActionMap` no longer calls `Input::IsKeyDown`
directly. Instead, it queries through the device manager:

```cpp
bool InputActionMap::IsSourceDown(const InputSource& source) const
{
    InputDevice* device = m_DeviceManager->GetDevice(
        source.SourceType == InputSource::Type::Key ? InputDevice::Type::Keyboard :
        source.SourceType == InputSource::Type::MouseButton ? InputDevice::Type::Mouse :
        InputDevice::Type::Gamepad,
        source.DeviceIndex);

    if (!device) return false;
    return device->GetInput(source.Code).AsBool();
}
```

**Design decisions**:

- `InputValue` is a small struct (two floats), not a variant. This keeps it simple
  and avoids branching on type.
- Devices are **polled**, not callback-driven. This matches our polling-first philosophy.
- The device abstraction is the prerequisite for Layer 10 (Recording/Replay) —
  a `ReplayDevice` can inject recorded state through the same interface.

---

## 8. Layer 6 — Event Bus (Signal/Slot)

**Goal**: A lightweight publish-subscribe system for discrete, system-level
notifications. NOT used for per-frame input polling.

### Design Principles

1. **Type-safe dispatch**: subscribers register for a specific event type `T`,
   receive `const T&`. No base class, no virtual functions, no downcasting.
2. **RAII lifetime**: subscription returns a `ScopedConnection` that auto-unsubscribes
   on destruction. No dangling callback risk.
3. **No global state**: `EventBus` is an instance (owned by `Application`),
   passed where needed.
4. **No allocation per event**: events are stack-constructed and passed by reference.
5. **Safe during dispatch**: subscribe/unsubscribe during `Publish()` must not
   crash or skip entries (deferred mutation).

### ScopedConnection.h

```cpp
#pragma once

#include <functional>
#include <memory>

// RAII connection handle. Destructor auto-unsubscribes.
class ScopedConnection
{
public:
    ScopedConnection() = default;

    explicit ScopedConnection(std::function<void()> unsub)
        : m_Unsubscribe(std::make_unique<std::function<void()>>(std::move(unsub)))
    {}

    ~ScopedConnection()
    {
        if (m_Unsubscribe && *m_Unsubscribe)
            (*m_Unsubscribe)();
    }

    // Move-only
    ScopedConnection(ScopedConnection&&) = default;
    ScopedConnection& operator=(ScopedConnection&&) = default;

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    void Disconnect()
    {
        if (m_Unsubscribe && *m_Unsubscribe)
        {
            (*m_Unsubscribe)();
            m_Unsubscribe.reset();
        }
    }

private:
    std::unique_ptr<std::function<void()>> m_Unsubscribe;
};
```

### EventBus.h

```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>

#include "ScopedConnection.h"

class EventBus
{
public:
    // Subscribe to events of type T. Returns an RAII handle.
    template<typename T>
    ScopedConnection Subscribe(std::function<void(const T&)> handler)
    {
        auto& subs = GetSubscribers<T>();
        uint64_t id = m_NextId++;

        subs.Entries.push_back({ id, std::move(handler), false });

        return ScopedConnection([this, id]()
        {
            // Deferred removal: mark for deletion, don't erase during iteration.
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
    }

    // Publish an event. All subscribers of type T are called synchronously.
    // Safe to subscribe/unsubscribe from within a handler.
    template<typename T>
    void Publish(const T& event)
    {
        auto& subs = GetSubscribers<T>();
        subs.DispatchDepth++;

        for (size_t i = 0; i < subs.Entries.size(); ++i)
        {
            auto& entry = subs.Entries[i];
            if (!entry.PendingRemoval)
                entry.Handler(event);
        }

        subs.DispatchDepth--;

        // Compact removed entries only when all nested dispatches are done.
        if (subs.DispatchDepth == 0 && subs.Dirty)
        {
            subs.Entries.erase(
                std::remove_if(subs.Entries.begin(), subs.Entries.end(),
                    [](const auto& e) { return e.PendingRemoval; }),
                subs.Entries.end());
            subs.Dirty = false;
        }
    }

private:
    struct ISubscriberList
    {
        virtual ~ISubscriberList() = default;
    };

    template<typename T>
    struct SubscriberEntry
    {
        uint64_t Id;
        std::function<void(const T&)> Handler;
        bool PendingRemoval = false;
    };

    template<typename T>
    struct SubscriberList : ISubscriberList
    {
        std::vector<SubscriberEntry<T>> Entries;
        int DispatchDepth = 0;   // Re-entrancy counter for nested Publish calls.
        bool Dirty = false;      // True if any entry is marked PendingRemoval.
    };

    template<typename T>
    SubscriberList<T>& GetSubscribers()
    {
        auto key = std::type_index(typeid(T));
        auto it = m_Subscribers.find(key);
        if (it == m_Subscribers.end())
        {
            auto list = std::make_unique<SubscriberList<T>>();
            auto* ptr = list.get();
            m_Subscribers[key] = std::move(list);
            return *ptr;
        }
        return *static_cast<SubscriberList<T>*>(it->second.get());
    }

    std::unordered_map<std::type_index, std::unique_ptr<ISubscriberList>> m_Subscribers;
    uint64_t m_NextId = 0;
};
```

**Key improvement over the previous version**: `DispatchDepth` counter + deferred
removal. If a handler calls `Disconnect()` (directly or via `ScopedConnection`
destructor) during `Publish()`, the entry is marked `PendingRemoval` instead of
being erased from the vector. Actual compaction happens after the outermost
`Publish()` returns. This prevents iterator invalidation and use-after-free.

**Known issue / design gap identified during review**:

- `DispatchDepth` + deferred removal addresses mid-dispatch mutation safety, but
  it does not by itself guarantee full lifetime safety. `ScopedConnection`
  currently stores an unsubscribe callable that captures `EventBus::this`; if a
  connection outlives the owning `EventBus`, destroying that connection could
  still invoke a dangling unsubscribe path. The architecture direction remains
  valid, but the final implementation should define and enforce clear lifetime
  rules or adopt a safer connection-token strategy.

### Event Types (Events.h)

Events are simple structs. No base class, no macros, no inheritance hierarchy.

```cpp
#pragma once

#include <cstdint>
#include "KeyCode.h"
#include "MouseCode.h"

// ---- Window events ----

struct WindowResizeEvent
{
    uint32_t Width;
    uint32_t Height;
};

struct WindowCloseEvent {};

// ---- Keyboard events (discrete, one-shot) ----
// NOTE: These are for systems that truly need callback-style notification
// (e.g., text input, ImGui routing). Normal gameplay input should use
// Input::WasKeyPressedThisFrame() instead.

struct KeyPressedEvent
{
    Key::Code   KeyCode;
    bool        IsRepeat;
};

struct KeyReleasedEvent
{
    Key::Code   KeyCode;
};

struct CharTypedEvent
{
    uint32_t    Codepoint;  // Unicode codepoint, for text input
};

// ---- Mouse events (discrete) ----

struct MouseButtonPressedEvent
{
    Mouse::Code Button;
};

struct MouseButtonReleasedEvent
{
    Mouse::Code Button;
};

struct MouseScrolledEvent
{
    float       XOffset;
    float       YOffset;
};

// ---- Application-level events ----

struct DemoSwitchedEvent
{
    int         NewIndex;
    const char* NewName;
};

struct RendererSettingsChangedEvent
{
    // Fields TBD based on SceneRendererSpecification
};

// ---- Input device events ----

struct GamepadConnectedEvent
{
    int         DeviceIndex;
    const char* DeviceName;
};

struct GamepadDisconnectedEvent
{
    int         DeviceIndex;
};
```

### Usage example

```cpp
// In Application or Window setup:
EventBus m_EventBus;

// In Window — GLFW key callback publishes events:
glfwSetKeyCallback(m_Handle, [](GLFWwindow* window, int key, int scancode,
                                 int action, int mods)
{
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (action == GLFW_PRESS)
        self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), false });
    else if (action == GLFW_REPEAT)
        self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), true });
    else if (action == GLFW_RELEASE)
        self->m_EventBus->Publish(KeyReleasedEvent{ static_cast<Key::Code>(key) });
});

// In ImGuiLayer — subscribe to block input when ImGui wants it:
void ImGuiLayer::OnAttach()
{
    m_OnKeyPressed = m_EventBus->Subscribe<KeyPressedEvent>(
        [this](const KeyPressedEvent& e)
        {
            // ImGui gets key events through imgui_impl_glfw directly.
            // This handler is for future use (e.g., debug console toggle).
        });
}

// In some other system — subscribe to window resize:
m_OnResize = eventBus.Subscribe<WindowResizeEvent>(
    [this](const WindowResizeEvent& e)
    {
        m_Camera.SetViewportSize(e.Width, e.Height);
    });
```

**Design decisions**:

- **No `Handled` flag**. Unlike Cherno-style, there is no event "consumption"
  mechanism in the bus itself. If ImGui needs to block input from reaching game
  logic, that happens at the **Input System** level (capture flags on `Input`),
  not by stopping event propagation.
- **No `EventCategory` bitfield**. With type-safe `Subscribe<T>`, you only
  receive what you asked for.
- **Synchronous dispatch**. `Publish()` calls handlers inline. For the current
  scale this is appropriate.

**Future extensions**:

- **Event queue**: ring buffer stores events, `FlushEvents()` dispatches at a
  defined point in the frame. Useful for deterministic ordering and cross-thread
  scenarios.
- **Priority subscribers**: handlers with higher priority are called first,
  can optionally stop propagation to lower-priority handlers.

---

## 9. Layer 7 — Window Event Production

**Goal**: `Window` registers GLFW callbacks that produce events into the `EventBus`.

### Window changes

```cpp
class Window
{
public:
    // Existing API...

    // New: give Window a reference to the EventBus so it can publish events.
    void SetEventBus(EventBus* bus);

private:
    EventBus* m_EventBus = nullptr;

    // Called from Init() after GLFW window creation.
    void InstallCallbacks();
};
```

### InstallCallbacks sketch

```cpp
void Window::InstallCallbacks()
{
    glfwSetFramebufferSizeCallback(m_Handle, [](GLFWwindow* w, int width, int height)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        self->m_Width  = static_cast<uint32_t>(width);
        self->m_Height = static_cast<uint32_t>(height);
        if (self->m_EventBus)
            self->m_EventBus->Publish(WindowResizeEvent{ self->m_Width, self->m_Height });
    });

    glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow* w)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self->m_EventBus)
            self->m_EventBus->Publish(WindowCloseEvent{});
    });

    glfwSetKeyCallback(m_Handle, [](GLFWwindow* w, int key, int /*scancode*/,
                                     int action, int /*mods*/)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self->m_EventBus) return;

        if (action == GLFW_PRESS)
            self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), false });
        else if (action == GLFW_REPEAT)
            self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), true });
        else if (action == GLFW_RELEASE)
            self->m_EventBus->Publish(KeyReleasedEvent{ static_cast<Key::Code>(key) });
    });

    glfwSetCharCallback(m_Handle, [](GLFWwindow* w, unsigned int codepoint)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self->m_EventBus)
            self->m_EventBus->Publish(CharTypedEvent{ codepoint });
    });

    glfwSetMouseButtonCallback(m_Handle, [](GLFWwindow* w, int button,
                                             int action, int /*mods*/)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self->m_EventBus) return;

        if (action == GLFW_PRESS)
            self->m_EventBus->Publish(MouseButtonPressedEvent{ static_cast<Mouse::Code>(button) });
        else
            self->m_EventBus->Publish(MouseButtonReleasedEvent{ static_cast<Mouse::Code>(button) });
    });

    glfwSetScrollCallback(m_Handle, [](GLFWwindow* w, double xoffset, double yoffset)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self->m_EventBus)
            self->m_EventBus->Publish(MouseScrolledEvent{
                static_cast<float>(xoffset),
                static_cast<float>(yoffset)
            });
        Input::AccumulateScroll(static_cast<float>(yoffset));
    });
}
```

---

## 10. Layer 8 — ImGui Input Routing

**Goal**: When ImGui wants keyboard or mouse focus, block the Input polling layer
so game/demo code doesn't respond to keys that are meant for UI widgets.

### Approach: capture flag on Input, not event consumption

```cpp
// Called once per frame, after ImGui::NewFrame().
void ImGuiLayer::UpdateInputCapture()
{
    ImGuiIO& io = ImGui::GetIO();
    Input::SetKeyboardCaptured(io.WantCaptureKeyboard);
    Input::SetMouseCaptured(io.WantCaptureMouse);
}
```

```cpp
// In Input.cpp — IsKeyDown respects capture:
bool Input::IsKeyDown(Key::Code key)
{
    if (s_KeyboardCaptured) return false;
    return key < KEY_STATE_SIZE && s_CurrentKeys[key];
}
```

**Why this instead of event-based `Handled` flags?**

- Simpler. One flag per device, checked in one place.
- No ordering dependency on which layer processes events first.
- ImGui's `WantCaptureKeyboard` / `WantCaptureMouse` is already the
  authoritative source — we just forward it to the polling layer.

**Limitation**: This only distinguishes "ImGui vs everything else". When
the Input Context Stack (Layer 4) is implemented, the capture flag should
integrate with it — ImGui becomes the highest-priority context that
conditionally consumes all input.

---

## 11. Layer 9 — Chord, Combo & Gesture Recognition

**Goal**: Support multi-key combinations, sequential inputs, and time-sensitive
gestures as first-class action bindings.

### Chord (simultaneous keys)

A chord is "all of these keys must be held at the same time":

```cpp
// Ctrl+S to save, Ctrl+Shift+Z to redo
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

**Key subtlety**: When `Ctrl+S` is bound as a chord, pressing `Ctrl` alone
should NOT trigger any action bound to just `Ctrl`. This requires the context
stack to check "is this key part of a pending chord?" before firing single-key
actions. Unreal handles this with **implicit chord blocking**.

### Combo (sequential inputs)

A combo is "press these keys in this order within a time window":

```cpp
struct ComboBinding
{
    struct Step
    {
        InputSource Source;
        float       MaxDelay;   // Max time to wait before this step must occur
    };

    std::vector<Step> Steps;
};

// State machine for combo evaluation:
class ComboTracker
{
public:
    void SetCombo(const ComboBinding& combo);

    // Returns true on the frame the full combo completes.
    bool Update(float dt);

    void Reset();

private:
    ComboBinding m_Combo;
    size_t m_CurrentStep = 0;
    float  m_StepTimer = 0.0f;
};
```

### Double-tap

A common gesture: press the same key twice within a short window.

```cpp
class DoubleTapTrigger : public InputTrigger
{
public:
    explicit DoubleTapTrigger(float maxGap = 0.3f) : m_MaxGap(maxGap) {}

    TriggerState Evaluate(bool down, bool pressed, bool released, float dt) override
    {
        if (pressed)
        {
            if (m_WaitingForSecond && m_GapTimer <= m_MaxGap)
            {
                m_WaitingForSecond = false;
                return TriggerState::Triggered;
            }
            m_WaitingForSecond = true;
            m_GapTimer = 0.0f;
        }

        if (m_WaitingForSecond)
        {
            m_GapTimer += dt;
            if (m_GapTimer > m_MaxGap)
                m_WaitingForSecond = false;
        }

        return TriggerState::None;
    }

    void Reset() override { m_WaitingForSecond = false; m_GapTimer = 0.0f; }

private:
    float m_MaxGap;
    float m_GapTimer = 0.0f;
    bool  m_WaitingForSecond = false;
};
```

### Integration with InputActionMap

```cpp
// Bind a chord action
m_InputMap.BindChord("Redo", { Key::LeftControl, Key::LeftShift, Key::Z });

// Bind with double-tap trigger
m_InputMap.BindAction("DashForward", Key::W);
m_InputMap.SetTrigger("DashForward", std::make_unique<DoubleTapTrigger>(0.25f));

// Bind a combo (fighting-game style)
m_InputMap.BindCombo("SpecialAttack", {
    { InputSource::FromKey(Key::Down),  0.3f },
    { InputSource::FromKey(Key::Right), 0.3f },
    { InputSource::FromKey(Key::P),     0.2f }
});
```

**Design decisions**:

- Chords, combos, and gestures are all built on top of the existing
  `InputActionMap` + `InputTrigger` infrastructure. They don't require
  a separate system.
- Chord blocking (suppressing single-key actions while a chord is pending)
  is the most complex part. It lives in `InputContextStack::Update()` which
  has visibility into all active contexts and their bindings.

---

## 12. Layer 10 — Input Recording & Replay

**Goal**: Capture per-frame input state to a file, then replay it deterministically.
Useful for automated testing, bug reproduction, and demo playback.

### Architecture

The key insight is that with the Device Abstraction (Layer 5), recording and
replay become a matter of swapping device implementations:

```
Normal:   GLFWwindow → KeyboardDevice → Input → ActionMap → Game
Record:   GLFWwindow → KeyboardDevice → RecordingTee → Input → ActionMap → Game
                                         ↓
                                    Recording File

Replay:   Recording File → ReplayDevice → Input → ActionMap → Game
```

### InputRecorder

```cpp
#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct InputFrame
{
    uint64_t    FrameNumber;
    float       DeltaTime;

    // Keyboard state snapshot (compressed: only store keys that are down)
    std::vector<Key::Code>      KeysDown;

    // Mouse state
    float       MouseX, MouseY;
    float       MouseDeltaX, MouseDeltaY;
    float       ScrollDelta;
    uint8_t     MouseButtonsDown;   // Bitmask of Mouse::Button0..Button7
};

class InputRecorder
{
public:
    void BeginRecording();
    void EndRecording();

    // Called each frame to capture current input state.
    void CaptureFrame(uint64_t frameNumber, float dt);

    // Save/load to binary file.
    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);

    const std::vector<InputFrame>& GetFrames() const { return m_Frames; }

private:
    std::vector<InputFrame> m_Frames;
    bool m_Recording = false;
};
```

### ReplayDevice

```cpp
// Implements InputDevice interface, feeds recorded state instead of live input.
class ReplayDevice : public InputDevice
{
public:
    explicit ReplayDevice(const std::vector<InputFrame>& frames);

    Type GetType() const override { return Type::Custom; }

    void Poll() override
    {
        // Advance to next frame's recorded state.
        if (m_CurrentFrame < m_Frames.size())
            m_CurrentFrame++;
    }

    InputValue GetInput(uint16_t code) const override
    {
        if (m_CurrentFrame >= m_Frames.size()) return {};
        const auto& frame = m_Frames[m_CurrentFrame];
        bool down = std::find(frame.KeysDown.begin(), frame.KeysDown.end(), code)
                    != frame.KeysDown.end();
        return { down ? 1.0f : 0.0f, 0.0f };
    }

    bool IsFinished() const { return m_CurrentFrame >= m_Frames.size(); }

private:
    const std::vector<InputFrame>& m_Frames;
    size_t m_CurrentFrame = 0;
};
```

### Determinism requirements

For replay to work correctly:

1. **Fixed timestep** or recorded delta times must be replayed exactly.
2. **No external randomness** in input-dependent code paths (or seed must be recorded).
3. **Frame-accurate state** — all input queries must go through the device
   abstraction, no direct GLFW calls in game logic.

**Design decisions**:

- Recording is per-frame state snapshots, not per-event. This matches the
  polling-first architecture and avoids timing issues with callback ordering.
- Binary format for compactness. A 60fps recording for 1 minute is ~3600 frames,
  roughly 100KB uncompressed.
- The recorder is optional — it only needs to exist when you're debugging or
  testing. It has zero overhead when not active.

**Known issue / design gap identified during review**:

- The current replay design is clearly scoped around polling-based gameplay
  input state, but the boundary is not yet fully defined relative to discrete
  engine/platform events such as `CharTypedEvent`, `WindowResizeEvent`,
  `WindowCloseEvent`, or device connect/disconnect notifications. If the long-
  term goal includes editor/tooling parity with modern engines, the document
  should eventually clarify whether replay covers gameplay input only, or also
  needs a parallel strategy for reproducing discrete event traffic.

---

## 13. Data Flow Diagram

### Core flow (Layers 0-2, 6)

```
┌─────────────────────────────────────────────────────────────────────┐
│                        GLFW Callbacks                               │
│   key / mouse button / scroll / char / resize / close               │
└──────────┬────────────────────────────────┬─────────────────────────┘
           │                                │
           ▼                                ▼
┌──────────────────────┐         ┌──────────────────────┐
│   Input (Layer 1)    │         │  EventBus (Layer 6)  │
│                      │         │                      │
│ • BeginFrame() polls │         │ • Publish<T>() from  │
│   GLFW state         │         │   GLFW callbacks     │
│ • Double-buffer for  │         │ • Subscribers get    │
│   edge detection     │         │   const T&           │
│ • Scroll accumulator │         │ • RAII connections   │
│   fed by callback    │         │ • Deferred removal   │
└──────────┬───────────┘         └──────────┬───────────┘
           │                                │
           ▼                                ▼
┌──────────────────────┐         ┌──────────────────────┐
│ InputActionMap       │         │ Subscribers:         │
│ (Layer 2)            │         │ • ImGuiLayer         │
│                      │         │ • Application        │
│ • "MoveForward" axis │         │   (WindowResize,     │
│ • "ShowShadowMap"    │         │    WindowClose)      │
│   action             │         │ • Future systems     │
│ • Queries Input      │         │                      │
│   polling internally │         │                      │
└──────────┬───────────┘         └──────────────────────┘
           │
           ▼
┌──────────────────────┐
│ Demo / Layer code    │
│                      │
│ m_InputMap.GetAxis() │
│ m_InputMap.WasAction │
│   PressedThisFrame() │
└──────────────────────┘
```

### Full flow (all layers)

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          Platform / GLFW                                 │
└──────┬───────────────────────────────────────────┬───────────────────────┘
       │                                           │
       ▼                                           ▼
┌──────────────────┐                    ┌──────────────────────┐
│ InputDeviceManager│ (Layer 5)         │  Window callbacks    │ (Layer 7)
│                  │                    │  → EventBus          │
│ KeyboardDevice   │                    └──────────┬───────────┘
│ MouseDevice      │                               │
│ GamepadDevice    │                               ▼
│ (ReplayDevice)   │                    ┌──────────────────────┐
└──────┬───────────┘                    │  EventBus (Layer 6)  │
       │ Poll()                         │                      │
       ▼                                │  WindowResize        │
┌──────────────────┐                    │  WindowClose         │
│ Input (Layer 1)  │                    │  GamepadConnected    │
│ Double-buffered  │                    │  DemoSwitched        │
│ state + capture  │                    └──────────────────────┘
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│ InputActionMap   │ (Layer 2)
│ Actions + Axes   │
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│ Modifiers &      │ (Layer 3)
│ Triggers         │
│ DeadZone, Hold,  │
│ Tap, Sensitivity │
└──────┬───────────┘
       │
       ▼
┌──────────────────┐      ┌─────────────────────┐
│ InputContext      │ ←──→ │ ImGui Capture       │ (Layer 8)
│ Stack (Layer 4)  │      │ SetKeyboardCaptured │
│                  │      └─────────────────────┘
│ [PauseMenu:100]  │
│ [DebugConsole:50]│
│ [Gameplay:0]     │
└──────┬───────────┘
       │
       ▼
┌──────────────────┐      ┌─────────────────────┐
│ Chord / Combo /  │ ←──→ │ Input Recording     │ (Layer 10)
│ Gesture (Layer 9)│      │ Capture / Replay    │
└──────┬───────────┘      └─────────────────────┘
       │
       ▼
┌──────────────────┐
│ Game / Demo code │
│                  │
│ Fully abstracted │
│ from physical    │
│ input sources    │
└──────────────────┘
```

**Key insight**: The Input polling path (left) and Event bus path (right) are
**independent**. They share GLFW as the source, but serve different consumers.
Game logic uses the left path. System notifications use the right path.

---

## 14. File Layout

```
src/
├── core/
│   ├── input/
│   │   ├── KeyCode.h                          # Layer 0
│   │   ├── MouseCode.h                        # Layer 0
│   │   ├── Input.h / Input.cpp                # Layer 1
│   │   ├── InputAction.h / InputAction.cpp    # Layer 2
│   │   ├── InputModifier.h                    # Layer 3 (header-only)
│   │   ├── InputTrigger.h / InputTrigger.cpp  # Layer 3
│   │   ├── InputContext.h / InputContext.cpp   # Layer 4
│   │   ├── InputDevice.h                      # Layer 5 (interface)
│   │   ├── KeyboardDevice.h / .cpp            # Layer 5
│   │   ├── MouseDevice.h / .cpp               # Layer 5
│   │   ├── GamepadDevice.h / .cpp             # Layer 5
│   │   ├── InputDeviceManager.h / .cpp        # Layer 5
│   │   ├── ChordBinding.h                     # Layer 9
│   │   ├── ComboTracker.h / .cpp              # Layer 9
│   │   ├── InputRecorder.h / .cpp             # Layer 10
│   │   └── ReplayDevice.h / .cpp              # Layer 10
│   ├── events/
│   │   ├── ScopedConnection.h                 # Layer 6
│   │   ├── EventBus.h                         # Layer 6 (header-only, templated)
│   │   └── Events.h                           # Event struct definitions
│   ├── Window.h / Window.cpp                  # Layer 7 (publishes events)
│   ├── Application.h / Application.cpp        # Owns EventBus, calls BeginFrame
│   └── ... (existing files unchanged)
├── gui/
│   ├── ImGuiLayer.h / ImGuiLayer.cpp          # Layer 8
│   └── ...
```

---

## 15. Phased Implementation Plan

### Phase A — Foundation (current project stage)

Implement now. Prerequisite for everything else.

| Item | Layer | Rationale | Effort |
|------|-------|-----------|--------|
| `KeyCode.h` | 0 | Removes raw ints and GLFW header leaks from demo code | ~30 min |
| `MouseCode.h` | 0 | Same as above | ~10 min |
| Revise `Input` signatures | 1 | `int` → `Key::Code` / `Mouse::Code` | ~30 min |
| `BeginFrame()` + double buffer | 1 | Enables edge detection, fixes the key-1/2 bug | ~1 hr |
| `WasKeyPressedThisFrame` | 1 | Immediate payoff in ShadowMapping demo | included above |

**After this**: ShadowMapping demo no longer has `constexpr int KEY_W = 87` hacks,
output mode switching works correctly (one toggle per press), and the Input API
is clean for all future demos.

### Phase B — ImGuiLayer integration

Implement when building ImGuiLayer.

| Item | Layer | Rationale |
|------|-------|-----------|
| `ScopedConnection.h` | 6 | RAII event connection lifetime |
| `EventBus.h` (with deferred removal) | 6 | ImGuiLayer + Application need pub/sub |
| `Events.h` (struct definitions) | 6 | WindowResize, KeyPressed, etc. |
| Window callback installation | 7 | Produce events from GLFW into the bus |
| `Input` capture flags | 8 | Block polling when ImGui has focus |
| Migrate `Window::m_ResizeCallback` to EventBus | 7 | Unify notification path |

### Phase C — Action mapping

Implement when adding second demo or runtime settings.

| Item | Layer | Rationale |
|------|-------|-----------|
| `InputActionMap` | 2 | Demos define their own bindings |
| `DemoSwitchedEvent` | 6 | Clean demo change notification |
| `RendererSettingsChangedEvent` | 6 | DebugPanel tweaks propagate cleanly |

### Phase D — Input processing pipeline

Implement when polish matters (sensitivity settings, UI for input tuning).

| Item | Layer | Rationale |
|------|-------|-----------|
| `InputModifier` base + built-ins | 3 | DeadZone, Sensitivity, Clamp, Negate, Smooth |
| `InputTrigger` base + built-ins | 3 | Hold, Tap, Released triggers |
| `InputActionMap::Update(dt)` | 3 | Advance trigger state machines per frame |

### Phase E — Context stack

Implement when multiple UI layers coexist (pause menu, debug console, gameplay).

| Item | Layer | Rationale |
|------|-------|-----------|
| `InputContextStack` | 4 | Priority-based action map activation |
| ImGui as highest-priority context | 4+8 | Replace simple capture flags |
| Per-context `ConsumesInput` logic | 4 | Modal UI blocks gameplay input |

### Phase F — Device abstraction

Implement when adding gamepad support or preparing for replay.

| Item | Layer | Rationale |
|------|-------|-----------|
| `InputDevice` interface | 5 | Abstract away GLFW dependency |
| `KeyboardDevice`, `MouseDevice` | 5 | Concrete implementations |
| `InputDeviceManager` | 5 | Device lifecycle and polling |
| `GamepadDevice` | 5 | GLFW joystick/gamepad API |
| Gamepad hot-plug events | 5+6 | `GamepadConnectedEvent` / `DisconnectedEvent` |
| Refactor `InputActionMap` to query devices | 5 | Remove direct `Input::` static calls |

### Phase G — Advanced input patterns

Implement when game mechanics demand it.

| Item | Layer | Rationale |
|------|-------|-----------|
| `ChordBinding` | 9 | Ctrl+S, Ctrl+Shift+Z style shortcuts |
| Chord blocking in context stack | 9+4 | Prevent single-key fire during chord |
| `ComboTracker` | 9 | Sequential input patterns |
| `DoubleTapTrigger` | 9+3 | Double-tap to dash / activate |

### Phase H — Recording & replay

Implement when automated testing or demo playback is needed.

| Item | Layer | Rationale |
|------|-------|-----------|
| `InputRecorder` | 10 | Capture per-frame input snapshots |
| `ReplayDevice` | 10 | Feed recorded state through device abstraction |
| Binary save/load | 10 | Persist recordings to disk |
| Deterministic replay validation | 10 | Verify frame-accurate reproduction |

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
| Input abstraction | Direct GLFW calls | Device abstraction layer |
| Action mapping | None (raw key codes in game logic) | `InputActionMap` with modifiers and triggers |
| Multi-layer input | LayerStack linear propagation | `InputContextStack` with priorities |

---

## 17. Appendix B: Reference — How Production Engines Do It

### Unreal Engine — Enhanced Input

| Concept | UE Implementation | Our Equivalent |
|---------|-------------------|----------------|
| `UInputMappingContext` | Maps actions to keys, with priority | `InputActionMap` + `InputContextStack` |
| `UInputAction` | Defines a named action with value type | Action entries in `InputActionMap` |
| `UInputModifier` | DeadZone, Negate, Scalar, FOVScaling, etc. | `InputModifier` subclasses |
| `UInputTrigger` | Down, Pressed, Released, Hold, Tap, Combo, ChordAction | `InputTrigger` subclasses |
| `AddMappingContext(ctx, priority)` | Pushes context with priority | `InputContextStack::Push()` |
| Implicit chord blocking | Higher-priority chord blocks lower single-key | `InputContextStack` chord awareness |

### Unity — Input System

| Concept | Unity Implementation | Our Equivalent |
|---------|---------------------|----------------|
| `InputActionAsset` | JSON/YAML asset defining actions + bindings | `InputActionMap` (code-defined, optionally serializable) |
| `InputAction.Enable()` / `Disable()` | Toggle action map | `InputContextStack::SetActive()` |
| `Interaction` | Hold, Tap, MultiTap, SlowTap, Press | `InputTrigger` |
| `Processor` | DeadZone, Normalize, Invert, Scale | `InputModifier` |
| `InputDevice` | Abstract device base class | `InputDevice` |
| `InputEventTrace` | Records input events to buffer | `InputRecorder` |

### Godot 4

| Concept | Godot Implementation | Our Equivalent |
|---------|---------------------|----------------|
| `InputMap` | Global action → key mapping | `InputActionMap` |
| `Input.is_action_pressed()` | Polling query | `InputActionMap::IsActionDown()` |
| `Input.is_action_just_pressed()` | Edge detection | `InputActionMap::WasActionPressedThisFrame()` |
| `_input(event)` / `_unhandled_input(event)` | Event propagation through scene tree | `EventBus` + context stack |
| `InputEventAction` | Action-level event | `Subscribe<ActionTriggeredEvent>` (future) |
| Dead zone | Per-action configurable | `DeadZone` modifier |
