#pragma once

/// @file Window.h
/// @brief Platform window abstraction backed by GLFW.
///
/// Responsibilities:
///   - Create an OS window with an OpenGL context
///   - Initialize GLFW (once) and load GL functions via Glad
///   - Register an OpenGL debug callback (non-Apple platforms)
///   - Publish framebuffer-resize and other events via EventBus
///
/// The Window does NOT own the Input system - Input is initialized separately
/// with the native GLFW handle via Input::Initialize().

#include <string>
#include <functional>
#include <cstdint>

struct GLFWwindow;
class EventBus;

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
    using RefreshCallback = std::function<void()>;

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

    /// Register a callback invoked when the window needs to be redrawn
    /// (e.g., during live resize on macOS).
    void SetRefreshCallback(RefreshCallback callback);

    /// Give Window a reference to the EventBus so GLFW callbacks can
    /// publish events (WindowResize, KeyPressed, MouseScrolled, etc.).
    void SetEventBus(EventBus *bus);

private:
    void Init(const WindowProps &props);
    void Shutdown();

    /// Install all GLFW callbacks that publish events into the EventBus.
    /// Called from SetEventBus() after the bus pointer is set.
    void InstallCallbacks();

private:
    GLFWwindow *m_Handle = nullptr; // Non-owning; destroyed manually in Shutdown().
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    bool m_VSync = true;

    RefreshCallback m_RefreshCallback;
    EventBus *m_EventBus = nullptr; // Non-owning. Lifetime managed by Application.
};
