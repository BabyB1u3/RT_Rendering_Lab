#pragma once

#include <utility>

struct GLFWwindow;

class Input
{
public:
    static void Initialize(GLFWwindow *window);

    static bool IsKeyPressed(int key);
    static bool IsMouseButtonPressed(int button);

    static std::pair<float, float> GetMousePosition();
    static std::pair<float, float> GetMouseDelta();

    static float GetMouseX();
    static float GetMouseY();

private:
    // Non-owning pointer. Input does not manage GLFWWindow lifetime.
    static GLFWwindow *s_Window;
    static float s_LastMouseX;
    static float s_LastMouseY;
    static bool s_FirstMouseSample;
};
