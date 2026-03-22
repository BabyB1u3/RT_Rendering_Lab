#pragma once

/// @file Window.h
/// @brief Platform window abstraction backed by GLFW.
///
/// Responsibilities:
///   - Create an OS window with an OpenGL context
///   - Initialize GLFW (once) and load GL functions via Glad
///   - Register an OpenGL debug callback (non-Apple platforms)
///   - Forward framebuffer-resize events to the Application via ResizeCallback
///
/// The Window does NOT own the Input system — Input is initialized separately
/// with the native GLFW handle via Input::Initialize().

#include <string>
#include <functional>
#include <cstdint>

struct GLFWwindow;

struct WindowProps
{
    std::string Title = "RTRLab";
    uint32_t Width = 1600;
    uint32_t Height = 900;
    bool VSync = true;
};

class Window
{
public:
    using ResizeCallback = std::function<void(uint32_t, uint32_t)>;

    explicit Window(const WindowProps &props = {});
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    /// Process pending OS/input events (non-blocking).
    void PollEvents();
    /// Present the back buffer to the screen.
    void SwapBuffers();
    /// True if the user requested the window to close (e.g., clicked the X button).
    bool ShouldClose() const;

    void SetVSync(bool enabled);
    bool IsVSync() const { return m_VSync; }

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    /// Return the underlying GLFW window pointer (needed by Input, ImGui, etc.).
    GLFWwindow *GetNativeHandle() const { return m_Handle; }

    /// Register a callback invoked when the framebuffer is resized.
    void SetResizeCallback(ResizeCallback callback);

private:
    void Init(const WindowProps &props);
    void Shutdown();

private:
    GLFWwindow *m_Handle = nullptr;  // Non-owning; destroyed manually in Shutdown().
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    bool m_VSync = true;

    ResizeCallback m_ResizeCallback;
};
