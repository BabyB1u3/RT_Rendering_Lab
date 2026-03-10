#pragma once

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

    void PollEvents();
    void SwapBuffers();
    bool ShouldClose() const;

    void SetVSync(bool enabled);
    bool IsVSync() const { return m_VSync; }

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    GLFWwindow *GetNativeHandle() const { return m_Handle; }

    void SetResizeCallback(ResizeCallback callback);

private:
    void Init(const WindowProps &props);
    void Shutdown();

private:
    // Opaque native handle. Lifetime managed manually.
    GLFWwindow *m_Handle = nullptr;
    // Must use custom deleter if using unique_ptr for GLFWwindow:
    // std::unique_ptr<GLFWwindow, GLFWwindowDeleter>
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    bool m_VSync = true;

    ResizeCallback m_ResizeCallback;
};
