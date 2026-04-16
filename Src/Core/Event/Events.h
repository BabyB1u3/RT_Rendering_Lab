#pragma once

/// @file Events.h
/// @brief Plain-struct event definitions for the EventBus.
///
/// Events are simple data carriers - no base class, no macros, no inheritance.
/// Subscribe to a specific type via EventBus::Subscribe<T>.

#include <cstdint>
#include "Core/Input/Device/InputDevice.h"
#include "Core/Input/Code/KeyCode.h"
#include "Core/Input/Code/MouseCode.h"

// ---- Window events ----

struct WindowResizeEvent
{
    uint32_t m_Width;
    uint32_t m_Height;
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
    Key::Code m_KeyCode;
    bool m_IsRepeat;
};

struct KeyReleasedEvent
{
    Key::Code m_KeyCode;
};

struct CharTypedEvent
{
    uint32_t m_Codepoint;
};

// ---- Mouse events (discrete) ----

struct MouseButtonPressedEvent
{
    Mouse::Code m_Button;
};

struct MouseButtonReleasedEvent
{
    Mouse::Code m_Button;
};

struct MouseScrolledEvent
{
    float m_XOffset;
    float m_YOffset;
};

// ---- Device lifecycle events ----
// These describe changes to the logical device topology managed by
// InputDeviceManager as well as the connected availability of a slot.

struct DeviceAttachedToSlotEvent
{
    InputDevice::Type m_DeviceType;
    uint8_t m_DeviceIndex;
};

struct DeviceDetachedFromSlotEvent
{
    InputDevice::Type m_DeviceType;
    uint8_t m_DeviceIndex;
};

struct DeviceConnectionChangedEvent
{
    InputDevice::Type m_DeviceType;
    uint8_t m_DeviceIndex;
    bool m_IsConnected;
};

// ---- Gamepad compatibility events ----
// These are convenience projections of DeviceConnectionChangedEvent for
// gamepad slots only. They do not describe slot attach/detach or physical-only
// hot-plug; they simply mean the logical gamepad slot became connected or
// disconnected.

struct GamepadConnectedEvent
{
    uint8_t m_DeviceIndex;
};

struct GamepadDisconnectedEvent
{
    uint8_t m_DeviceIndex;
};

// ---- Application-level events ----

struct DemoSwitchedEvent
{
    int m_NewIndex;
    const char* m_NewName;
};
