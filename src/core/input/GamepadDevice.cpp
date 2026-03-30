#include "core/input/GamepadDevice.h"

#include <GLFW/glfw3.h>

namespace
{
    int ToGlfwJoystickId(uint8_t deviceIndex)
    {
        return GLFW_JOYSTICK_1 + static_cast<int>(deviceIndex);
    }
}

GamepadDevice::GamepadDevice(uint8_t deviceIndex)
    : m_DeviceIndex(deviceIndex)
{
}

void GamepadDevice::Poll()
{
    m_PreviousConnected = m_Connected;
    m_PreviousButtons = m_CurrentButtons;
    m_PreviousAxes = m_CurrentAxes;

    const int joystickId = ToGlfwJoystickId(m_DeviceIndex);
    m_Connected = (glfwJoystickIsGamepad(joystickId) == GLFW_TRUE);

    m_CurrentButtons.fill(false);
    m_CurrentAxes.fill(0.0f);

    if (m_Connected)
    {
        GLFWgamepadstate state{};
        if (glfwGetGamepadState(joystickId, &state) == GLFW_TRUE)
        {
            for (uint16_t i = 0; i < GamepadButton::Count; ++i)
                m_CurrentButtons[i] = (state.buttons[i] == GLFW_PRESS);
            for (uint16_t i = 0; i < GamepadAxis::Count; ++i)
                m_CurrentAxes[i] = state.axes[i];
        }
        else
        {
            m_Connected = false;
        }
    }

    m_HasConnectionStateChanged = m_HasPolledOnce && (m_Connected != m_PreviousConnected);
    m_HasPolledOnce = true;
}

InputValue GamepadDevice::GetInput(uint16_t code) const
{
    if (code >= GamepadButton::Count)
        return {};
    return {m_CurrentButtons[code] ? 1.0f : 0.0f, 0.0f};
}

InputValue GamepadDevice::GetPreviousInput(uint16_t code) const
{
    if (code >= GamepadButton::Count)
        return {};
    return {m_PreviousButtons[code] ? 1.0f : 0.0f, 0.0f};
}

InputValue GamepadDevice::GetAxis(uint16_t axisId) const
{
    if (axisId >= GamepadAxis::Count)
        return {};
    return {m_CurrentAxes[axisId], 0.0f};
}

InputValue GamepadDevice::GetPreviousAxis(uint16_t axisId) const
{
    if (axisId >= GamepadAxis::Count)
        return {};
    return {m_PreviousAxes[axisId], 0.0f};
}

void GamepadDevice::Reset()
{
    m_Connected = false;
    m_PreviousConnected = false;
    m_HasConnectionStateChanged = false;
    m_HasPolledOnce = false;
    m_CurrentButtons.fill(false);
    m_PreviousButtons.fill(false);
    m_CurrentAxes.fill(0.0f);
    m_PreviousAxes.fill(0.0f);
}

void GamepadDevice::ApplyState(bool connected,
                               const std::array<bool, GamepadButton::Count> &buttons,
                               const std::array<float, GamepadAxis::Count> &axes)
{
    m_PreviousConnected = m_Connected;
    m_PreviousButtons = m_CurrentButtons;
    m_PreviousAxes = m_CurrentAxes;

    m_Connected = connected;
    m_CurrentButtons = buttons;
    m_CurrentAxes = axes;
    m_HasConnectionStateChanged = m_HasPolledOnce && (m_Connected != m_PreviousConnected);
    m_HasPolledOnce = true;
}
