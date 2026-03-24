#pragma once

/// @file Events.h
/// @brief Plain-struct event definitions for the EventBus.
///
/// Events are simple data carriers — no base class, no macros, no inheritance.
/// Subscribe to a specific type via EventBus::Subscribe<T>.

#include <cstdint>
#include "core/input/KeyCode.h"
#include "core/input/MouseCode.h"

// ---- Window events ----

struct WindowResizeEvent
{
    uint32_t Width;
    uint32_t Height;
};

struct WindowCloseEvent
{
};

// ---- Keyboard events (discrete, one-shot) ----
// NOTE: These are for systems that truly need callback-style notification
// (e.g., text input, debug console toggle). Normal gameplay input should
// use Input::WasKeyPressedThisFrame() instead.

struct KeyPressedEvent
{
    Key::Code KeyCode;
    bool IsRepeat;
};

struct KeyReleasedEvent
{
    Key::Code KeyCode;
};

struct CharTypedEvent
{
    uint32_t Codepoint;
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
    float XOffset;
    float YOffset;
};

// ---- Application-level events ----

struct DemoSwitchedEvent
{
    int NewIndex;
    const char *NewName;
};
