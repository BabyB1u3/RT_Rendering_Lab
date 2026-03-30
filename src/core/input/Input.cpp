#include "core/input/Input.h"

#include <memory>

#include "core/event/EventBus.h"
#include "core/input/GamepadDevice.h"
#include "core/input/InputDeviceManager.h"
#include "core/input/KeyboardDevice.h"
#include "core/input/MouseDevice.h"

GLFWwindow *Input::s_Window = nullptr;
EventBus *Input::s_EventBus = nullptr;
std::unique_ptr<InputDeviceManager> Input::s_DeviceManager;
bool Input::s_KeyboardCaptured = false;
bool Input::s_MouseCaptured = false;

namespace
{
    bool IsPressed(float value)
    {
        return value > 0.5f;
    }

    const InputDevice *GetDevice(InputDevice::Type type, uint8_t deviceIndex = 0)
    {
        const auto *manager = Input::TryGetDeviceManager();
        return manager ? manager->GetDevice(type, deviceIndex) : nullptr;
    }
}

void Input::Initialize(GLFWwindow *window)
{
    EnsureDevices(window);

    if (window != nullptr && s_Window == window)
        return;

    s_Window = window;
    ResetDevices();
}

void Input::SetEventBus(EventBus *bus)
{
    s_EventBus = bus;
    if (s_DeviceManager)
        s_DeviceManager->SetEventBus(bus);
}

void Input::BeginFrame()
{
    if (!s_DeviceManager)
        EnsureDevices(s_Window);

    s_DeviceManager->PollAll();
}

void Input::RegisterDevice(std::unique_ptr<InputDevice> device)
{
    EnsureDevices(s_Window);
    s_DeviceManager->AddDevice(std::move(device));
}

void Input::RestoreDefaultDevices()
{
    EnsureDevices(s_Window);

    s_DeviceManager->AddDevice(std::make_unique<KeyboardDevice>(s_Window));
    s_DeviceManager->AddDevice(std::make_unique<MouseDevice>(s_Window));

    for (uint8_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        s_DeviceManager->AddDevice(std::make_unique<GamepadDevice>(i));

    ResetDevices();
}

InputDeviceManager &Input::GetDeviceManager()
{
    if (!s_DeviceManager)
        EnsureDevices(s_Window);
    return *s_DeviceManager;
}

const InputDeviceManager *Input::TryGetDeviceManager()
{
    return s_DeviceManager.get();
}

bool Input::IsKeyDown(Key::Code key)
{
    if (s_KeyboardCaptured)
        return false;

    const auto *device = GetDevice(InputDevice::Type::Keyboard);
    return device && IsPressed(device->GetInput(key).X);
}

bool Input::WasKeyPressedThisFrame(Key::Code key)
{
    if (s_KeyboardCaptured)
        return false;

    const auto *device = GetDevice(InputDevice::Type::Keyboard);
    return device && IsPressed(device->GetInput(key).X) && !IsPressed(device->GetPreviousInput(key).X);
}

bool Input::WasKeyReleasedThisFrame(Key::Code key)
{
    if (s_KeyboardCaptured)
        return false;

    const auto *device = GetDevice(InputDevice::Type::Keyboard);
    return device && !IsPressed(device->GetInput(key).X) && IsPressed(device->GetPreviousInput(key).X);
}

bool Input::IsMouseButtonDown(Mouse::Code button)
{
    if (s_MouseCaptured)
        return false;

    const auto *device = GetDevice(InputDevice::Type::Mouse);
    return device && IsPressed(device->GetInput(button).X);
}

bool Input::WasMouseButtonPressedThisFrame(Mouse::Code button)
{
    if (s_MouseCaptured)
        return false;

    const auto *device = GetDevice(InputDevice::Type::Mouse);
    return device && IsPressed(device->GetInput(button).X) && !IsPressed(device->GetPreviousInput(button).X);
}

bool Input::WasMouseButtonReleasedThisFrame(Mouse::Code button)
{
    if (s_MouseCaptured)
        return false;

    const auto *device = GetDevice(InputDevice::Type::Mouse);
    return device && !IsPressed(device->GetInput(button).X) && IsPressed(device->GetPreviousInput(button).X);
}

std::pair<float, float> Input::GetMousePosition()
{
    const auto *device = GetDevice(InputDevice::Type::Mouse);
    if (!device)
        return {0.0f, 0.0f};

    return {
        device->GetAxis(MouseAxisId::PositionX).X,
        device->GetAxis(MouseAxisId::PositionY).X};
}

std::pair<float, float> Input::GetMouseDelta()
{
    if (s_MouseCaptured)
        return {0.0f, 0.0f};

    const auto *device = GetDevice(InputDevice::Type::Mouse);
    if (!device)
        return {0.0f, 0.0f};

    return {
        device->GetAxis(MouseAxisId::DeltaX).X,
        device->GetAxis(MouseAxisId::DeltaY).X};
}

float Input::GetMouseX()
{
    return GetMousePosition().first;
}

float Input::GetMouseY()
{
    return GetMousePosition().second;
}

float Input::GetScrollDelta()
{
    if (s_MouseCaptured)
        return 0.0f;

    const auto *device = GetDevice(InputDevice::Type::Mouse);
    return device ? device->GetAxis(MouseAxisId::ScrollY).X : 0.0f;
}

void Input::AccumulateScroll(float yOffset)
{
    if (auto *device = GetMouseDevice())
        device->AccumulateScroll(yOffset);
}

bool Input::IsGamepadConnected(uint8_t deviceIndex)
{
    const auto *device = GetDevice(InputDevice::Type::Gamepad, deviceIndex);
    return device && device->IsConnected();
}

bool Input::IsGamepadButtonDown(GamepadButton::Code button, uint8_t deviceIndex)
{
    const auto *device = GetDevice(InputDevice::Type::Gamepad, deviceIndex);
    return device && IsPressed(device->GetInput(button).X);
}

bool Input::WasGamepadButtonPressedThisFrame(GamepadButton::Code button, uint8_t deviceIndex)
{
    const auto *device = GetDevice(InputDevice::Type::Gamepad, deviceIndex);
    return device && IsPressed(device->GetInput(button).X) && !IsPressed(device->GetPreviousInput(button).X);
}

bool Input::WasGamepadButtonReleasedThisFrame(GamepadButton::Code button, uint8_t deviceIndex)
{
    const auto *device = GetDevice(InputDevice::Type::Gamepad, deviceIndex);
    return device && !IsPressed(device->GetInput(button).X) && IsPressed(device->GetPreviousInput(button).X);
}

float Input::GetGamepadAxis(GamepadAxis::Code axis, uint8_t deviceIndex)
{
    const auto *device = GetDevice(InputDevice::Type::Gamepad, deviceIndex);
    return device ? device->GetAxis(axis).X : 0.0f;
}

void Input::SetKeyboardCaptured(bool captured)
{
    s_KeyboardCaptured = captured;
}

void Input::SetMouseCaptured(bool captured)
{
    s_MouseCaptured = captured;
}

bool Input::IsKeyboardCaptured()
{
    return s_KeyboardCaptured;
}

bool Input::IsMouseCaptured()
{
    return s_MouseCaptured;
}

void Input::EnsureDevices(GLFWwindow *window)
{
    if (!s_DeviceManager)
    {
        s_DeviceManager = std::make_unique<InputDeviceManager>();
        s_DeviceManager->SetEventBus(s_EventBus);
    }

    if (!s_DeviceManager->GetDevice(InputDevice::Type::Keyboard))
        s_DeviceManager->AddDevice(std::make_unique<KeyboardDevice>(window));

    if (!s_DeviceManager->GetDevice(InputDevice::Type::Mouse))
        s_DeviceManager->AddDevice(std::make_unique<MouseDevice>(window));

    for (uint8_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
    {
        if (!s_DeviceManager->GetDevice(InputDevice::Type::Gamepad, i))
            s_DeviceManager->AddDevice(std::make_unique<GamepadDevice>(i));
    }
}

void Input::ResetDevices()
{
    if (!s_DeviceManager)
        return;

    if (auto *keyboard = GetKeyboardDevice())
        keyboard->SetWindow(s_Window);

    if (auto *mouse = GetMouseDevice())
        mouse->SetWindow(s_Window);

    s_DeviceManager->ResetAll();

    s_KeyboardCaptured = false;
    s_MouseCaptured = false;
}

void Input::ApplyPolledState(const PolledState &state)
{
    EnsureDevices(nullptr);
    if (auto *keyboard = GetKeyboardDevice())
        keyboard->ApplyState(state.Keys);
    if (auto *mouse = GetMouseDevice())
        mouse->ApplyState(state.MouseButtons, state.MouseX, state.MouseY);
}

void Input::ApplyGamepadState(uint8_t deviceIndex, const GamepadPolledState &state)
{
    EnsureDevices(nullptr);
    if (auto *gamepad = GetGamepadDevice(deviceIndex))
        gamepad->ApplyState(state.Connected, state.Buttons, state.Axes);
}

KeyboardDevice *Input::GetKeyboardDevice()
{
    auto *device = s_DeviceManager ? s_DeviceManager->GetDevice(InputDevice::Type::Keyboard) : nullptr;
    return dynamic_cast<KeyboardDevice *>(device);
}

MouseDevice *Input::GetMouseDevice()
{
    auto *device = s_DeviceManager ? s_DeviceManager->GetDevice(InputDevice::Type::Mouse) : nullptr;
    return dynamic_cast<MouseDevice *>(device);
}

GamepadDevice *Input::GetGamepadDevice(uint8_t deviceIndex)
{
    auto *device = s_DeviceManager ? s_DeviceManager->GetDevice(InputDevice::Type::Gamepad, deviceIndex) : nullptr;
    return dynamic_cast<GamepadDevice *>(device);
}
