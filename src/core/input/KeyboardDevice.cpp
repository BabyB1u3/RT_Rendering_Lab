#include "core/input/KeyboardDevice.h"

#include <GLFW/glfw3.h>

KeyboardDevice::KeyboardDevice(GLFWwindow *window)
    : m_Window(window)
{
}

void KeyboardDevice::Poll()
{
    m_PreviousKeys = m_CurrentKeys;

    if (!m_Window)
    {
        m_CurrentKeys.fill(false);
        return;
    }

    m_CurrentKeys.fill(false);
    for (int i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; ++i)
        m_CurrentKeys[i] = (glfwGetKey(m_Window, i) == GLFW_PRESS);
}

InputValue KeyboardDevice::GetInput(uint16_t code) const
{
    if (code >= KEY_STATE_SIZE)
        return {};
    return {m_CurrentKeys[code] ? 1.0f : 0.0f, 0.0f};
}

InputValue KeyboardDevice::GetPreviousInput(uint16_t code) const
{
    if (code >= KEY_STATE_SIZE)
        return {};
    return {m_PreviousKeys[code] ? 1.0f : 0.0f, 0.0f};
}

void KeyboardDevice::Reset()
{
    m_CurrentKeys.fill(false);
    m_PreviousKeys.fill(false);
}

void KeyboardDevice::ApplyState(const std::array<bool, KEY_STATE_SIZE> &keys)
{
    m_PreviousKeys = m_CurrentKeys;
    m_CurrentKeys = keys;
}
