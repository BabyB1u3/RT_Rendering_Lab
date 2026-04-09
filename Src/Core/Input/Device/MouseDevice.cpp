#include "Core/Input/Device/MouseDevice.h"

#include <GLFW/glfw3.h>

MouseDevice::MouseDevice(GLFWwindow *window)
    : m_Window(window)
{
}

void MouseDevice::Poll()
{
    m_PreviousButtons = m_CurrentButtons;
    m_PreviousDeltaX = m_MouseX - m_LastMouseX;
    m_PreviousDeltaY = m_MouseY - m_LastMouseY;

    if (!m_Window)
    {
        m_CurrentButtons.fill(false);
        m_LastMouseX = m_MouseX;
        m_LastMouseY = m_MouseY;
        m_PreviousScrollThisFrame = m_ScrollThisFrame;
        m_ScrollThisFrame = m_ScrollAccumulator;
        m_ScrollAccumulator = 0.0f;
        return;
    }

    for (int i = 0; i < BUTTON_COUNT; ++i)
        m_CurrentButtons[i] = (glfwGetMouseButton(m_Window, i) == GLFW_PRESS);

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(m_Window, &mx, &my);

    m_LastMouseX = m_MouseX;
    m_LastMouseY = m_MouseY;
    m_MouseX = static_cast<float>(mx);
    m_MouseY = static_cast<float>(my);

    if (m_FirstMouseSample)
    {
        m_LastMouseX = m_MouseX;
        m_LastMouseY = m_MouseY;
        m_FirstMouseSample = false;
    }

    m_PreviousScrollThisFrame = m_ScrollThisFrame;
    m_ScrollThisFrame = m_ScrollAccumulator;
    m_ScrollAccumulator = 0.0f;
}

InputValue MouseDevice::GetInput(uint16_t code) const
{
    if (code >= BUTTON_COUNT)
        return {};
    return {m_CurrentButtons[code] ? 1.0f : 0.0f, 0.0f};
}

InputValue MouseDevice::GetPreviousInput(uint16_t code) const
{
    if (code >= BUTTON_COUNT)
        return {};
    return {m_PreviousButtons[code] ? 1.0f : 0.0f, 0.0f};
}

InputValue MouseDevice::GetAxis(uint16_t axisId) const
{
    switch (axisId)
    {
    case MouseAxisId::PositionX:
        return {m_MouseX, 0.0f};
    case MouseAxisId::PositionY:
        return {m_MouseY, 0.0f};
    case MouseAxisId::DeltaX:
        return {m_MouseX - m_LastMouseX, 0.0f};
    case MouseAxisId::DeltaY:
        return {m_MouseY - m_LastMouseY, 0.0f};
    case MouseAxisId::ScrollY:
        return {m_ScrollThisFrame, 0.0f};
    default:
        return {};
    }
}

InputValue MouseDevice::GetPreviousAxis(uint16_t axisId) const
{
    switch (axisId)
    {
    case MouseAxisId::PositionX:
        return {m_LastMouseX, 0.0f};
    case MouseAxisId::PositionY:
        return {m_LastMouseY, 0.0f};
    case MouseAxisId::DeltaX:
        return {m_PreviousDeltaX, 0.0f};
    case MouseAxisId::DeltaY:
        return {m_PreviousDeltaY, 0.0f};
    case MouseAxisId::ScrollY:
        return {m_PreviousScrollThisFrame, 0.0f};
    default:
        return {};
    }
}

void MouseDevice::Reset()
{
    m_CurrentButtons.fill(false);
    m_PreviousButtons.fill(false);
    m_MouseX = 0.0f;
    m_MouseY = 0.0f;
    m_LastMouseX = 0.0f;
    m_LastMouseY = 0.0f;
    m_PreviousDeltaX = 0.0f;
    m_PreviousDeltaY = 0.0f;
    m_FirstMouseSample = true;
    m_ScrollAccumulator = 0.0f;
    m_ScrollThisFrame = 0.0f;
    m_PreviousScrollThisFrame = 0.0f;
}

void MouseDevice::AccumulateScroll(float yOffset)
{
    m_ScrollAccumulator += yOffset;
}

void MouseDevice::ApplyState(const std::array<bool, BUTTON_COUNT> &buttons, float x, float y)
{
    m_PreviousButtons = m_CurrentButtons;
    m_PreviousDeltaX = m_MouseX - m_LastMouseX;
    m_PreviousDeltaY = m_MouseY - m_LastMouseY;
    m_CurrentButtons = buttons;

    m_LastMouseX = m_MouseX;
    m_LastMouseY = m_MouseY;
    m_MouseX = x;
    m_MouseY = y;

    if (m_FirstMouseSample)
    {
        m_LastMouseX = m_MouseX;
        m_LastMouseY = m_MouseY;
        m_FirstMouseSample = false;
    }

    m_PreviousScrollThisFrame = m_ScrollThisFrame;
    m_ScrollThisFrame = m_ScrollAccumulator;
    m_ScrollAccumulator = 0.0f;
}
