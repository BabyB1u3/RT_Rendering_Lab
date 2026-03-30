#include "core/input/Input.h"

#include <cstring>

#include <GLFW/glfw3.h>

// --- Static member definitions ---

GLFWwindow *Input::s_Window = nullptr;

bool Input::s_CurrentKeys[KEY_STATE_SIZE] = {};
bool Input::s_PreviousKeys[KEY_STATE_SIZE] = {};

bool Input::s_CurrentMouseButtons[MOUSE_BUTTON_COUNT] = {};
bool Input::s_PreviousMouseButtons[MOUSE_BUTTON_COUNT] = {};

float Input::s_MouseX = 0.0f;
float Input::s_MouseY = 0.0f;
float Input::s_LastMouseX = 0.0f;
float Input::s_LastMouseY = 0.0f;
bool Input::s_FirstMouseSample = true;

float Input::s_ScrollAccumulator = 0.0f;
float Input::s_ScrollThisFrame = 0.0f;

bool Input::s_KeyboardCaptured = false;
bool Input::s_MouseCaptured = false;

// --- Lifecycle ---

void Input::Initialize(GLFWwindow *window)
{
    s_Window = window;
    s_MouseX = 0.0f;
    s_MouseY = 0.0f;
    s_LastMouseX = 0.0f;
    s_LastMouseY = 0.0f;
    s_FirstMouseSample = true;

    std::memset(s_CurrentKeys, 0, sizeof(s_CurrentKeys));
    std::memset(s_PreviousKeys, 0, sizeof(s_PreviousKeys));
    std::memset(s_CurrentMouseButtons, 0, sizeof(s_CurrentMouseButtons));
    std::memset(s_PreviousMouseButtons, 0, sizeof(s_PreviousMouseButtons));

    s_ScrollAccumulator = 0.0f;
    s_ScrollThisFrame = 0.0f;
    s_KeyboardCaptured = false;
    s_MouseCaptured = false;
}

Input::PolledState Input::PollWindowState(GLFWwindow *window)
{
    PolledState state{};

    // GLFW valid key range is GLFW_KEY_SPACE (32) .. GLFW_KEY_LAST (348).
    // Keys 0-31 and >348 are invalid and trigger GLFW error callbacks.
    for (int i = GLFW_KEY_SPACE; i <= GLFW_KEY_LAST; ++i)
        state.Keys[i] = (glfwGetKey(window, i) == GLFW_PRESS);

    for (int i = 0; i < MOUSE_BUTTON_COUNT; ++i)
        state.MouseButtons[i] = (glfwGetMouseButton(window, i) == GLFW_PRESS);

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    state.MouseX = static_cast<float>(mx);
    state.MouseY = static_cast<float>(my);
    return state;
}

void Input::ApplyPolledState(const PolledState &state)
{
    std::memcpy(s_PreviousKeys, s_CurrentKeys, sizeof(s_CurrentKeys));
    std::memcpy(s_CurrentKeys, state.Keys.data(), sizeof(s_CurrentKeys));

    std::memcpy(s_PreviousMouseButtons, s_CurrentMouseButtons, sizeof(s_CurrentMouseButtons));
    std::memcpy(s_CurrentMouseButtons, state.MouseButtons.data(), sizeof(s_CurrentMouseButtons));

    s_LastMouseX = s_MouseX;
    s_LastMouseY = s_MouseY;
    s_MouseX = state.MouseX;
    s_MouseY = state.MouseY;

    if (s_FirstMouseSample)
    {
        s_LastMouseX = s_MouseX;
        s_LastMouseY = s_MouseY;
        s_FirstMouseSample = false;
    }

    s_ScrollThisFrame = s_ScrollAccumulator;
    s_ScrollAccumulator = 0.0f;
}

void Input::BeginFrame()
{
    if (!s_Window)
        return;

    ApplyPolledState(PollWindowState(s_Window));
}

// --- Keyboard ---

bool Input::IsKeyDown(Key::Code key)
{
    if (s_KeyboardCaptured)
        return false;
    return key < KEY_STATE_SIZE && s_CurrentKeys[key];
}

bool Input::WasKeyPressedThisFrame(Key::Code key)
{
    if (s_KeyboardCaptured)
        return false;
    return key < KEY_STATE_SIZE && s_CurrentKeys[key] && !s_PreviousKeys[key];
}

bool Input::WasKeyReleasedThisFrame(Key::Code key)
{
    if (s_KeyboardCaptured)
        return false;
    return key < KEY_STATE_SIZE && !s_CurrentKeys[key] && s_PreviousKeys[key];
}

// --- Mouse buttons ---

bool Input::IsMouseButtonDown(Mouse::Code button)
{
    if (s_MouseCaptured)
        return false;
    return button < MOUSE_BUTTON_COUNT && s_CurrentMouseButtons[button];
}

bool Input::WasMouseButtonPressedThisFrame(Mouse::Code button)
{
    if (s_MouseCaptured)
        return false;
    return button < MOUSE_BUTTON_COUNT && s_CurrentMouseButtons[button] && !s_PreviousMouseButtons[button];
}

bool Input::WasMouseButtonReleasedThisFrame(Mouse::Code button)
{
    if (s_MouseCaptured)
        return false;
    return button < MOUSE_BUTTON_COUNT && !s_CurrentMouseButtons[button] && s_PreviousMouseButtons[button];
}

// --- Mouse position & delta ---

std::pair<float, float> Input::GetMousePosition()
{
    return {s_MouseX, s_MouseY};
}

std::pair<float, float> Input::GetMouseDelta()
{
    if (s_MouseCaptured)
        return {0.0f, 0.0f};
    return {s_MouseX - s_LastMouseX, s_MouseY - s_LastMouseY};
}

float Input::GetMouseX()
{
    return s_MouseX;
}

float Input::GetMouseY()
{
    return s_MouseY;
}

// --- Scroll ---

float Input::GetScrollDelta()
{
    if (s_MouseCaptured)
        return 0.0f;
    return s_ScrollThisFrame;
}

void Input::AccumulateScroll(float yOffset)
{
    s_ScrollAccumulator += yOffset;
}

// --- Capture flags ---

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
