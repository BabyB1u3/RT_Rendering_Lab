#include "core/Input.h"

#include <GLFW/glfw3.h>

GLFWwindow *Input::s_Window = nullptr;
float Input::s_LastMouseX = 0.0f;
float Input::s_LastMouseY = 0.0f;
bool Input::s_FirstMouseSample = true;

void Input::Initialize(GLFWwindow *window)
{
    s_Window = window;
    s_FirstMouseSample = true;
}

bool Input::IsKeyPressed(int key)
{
    if (!s_Window)
        return false;

    const int state = glfwGetKey(s_Window, key);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsMouseButtonPressed(int button)
{
    if (!s_Window)
        return false;

    return glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
}

std::pair<float, float> Input::GetMousePosition()
{
    if (!s_Window)
        return {0.0f, 0.0f};

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(s_Window, &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

std::pair<float, float> Input::GetMouseDelta()
{
    auto [x, y] = GetMousePosition();

    if (s_FirstMouseSample)
    {
        s_LastMouseX = x;
        s_LastMouseY = y;
        s_FirstMouseSample = false;
        return {0.0f, 0.0f};
    }

    const float dx = x - s_LastMouseX;
    const float dy = y - s_LastMouseY;

    s_LastMouseX = x;
    s_LastMouseY = y;

    return {dx, dy};
}

float Input::GetMouseX()
{
    return GetMousePosition().first;
}

float Input::GetMouseY()
{
    return GetMousePosition().second;
}
