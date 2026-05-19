#include "Core/App/Window/Window.h"

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include "Core/App/Window/Native/Win32/Win32NativeWindow.h"
#elif defined(__APPLE__)
#include "Core/App/Window/Native/Cocoa/CocoaNativeWindow.h"
#elif defined(__linux__)
#include "Core/App/Window/Native/Linux/LinuxNativeWindow.h"
#endif
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Event/EventBus.h"
#include "Core/Event/Events.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Input/Input.h"

namespace
{
bool g_GlfwInitialized = false;

void OnGLFWError(int error, const char* description)
{
    LOG_ERROR_CAT(LogCategory::k_Window, "[GLFW Error] ({}): {}", error, description);
}
} // namespace

Window::Window(const WindowProps& props)
{
    Init(props);
}

Window::~Window()
{
    Shutdown();
}

void Window::Init(const WindowProps& props)
{
    m_Width = props.m_Width;
    m_Height = props.m_Height;

    if (!g_GlfwInitialized)
    {
        glfwSetErrorCallback(OnGLFWError);

        RTRLAB_ASSERT_MSG(glfwInit(), "Failed to initialize GLFW.");

        LOG_INFO_CAT(LogCategory::k_Window, "GLFW initialized");
        g_GlfwInitialized = true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_Handle = glfwCreateWindow(
        static_cast<int>(props.m_Width), static_cast<int>(props.m_Height), props.m_Title.c_str(), nullptr, nullptr);

    RTRLAB_ASSERT_MSG(m_Handle, "Failed to create GLFW window.");

    // On HiDPI/Retina displays the framebuffer size (physical pixels) differs from
    // the window size (logical points). Always store physical pixel dimensions so that
    // viewport setup is correct from the very first frame.
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(m_Handle, &fbWidth, &fbHeight);
    m_Width = static_cast<uint32_t>(fbWidth);
    m_Height = static_cast<uint32_t>(fbHeight);

    LOG_INFO_CAT(LogCategory::k_Window,
                 "Window created: {}x{} (framebuffer {}x{}) \"{}\"",
                 props.m_Width,
                 props.m_Height,
                 m_Width,
                 m_Height,
                 props.m_Title);

    // Store 'this' in the GLFW window so that static callbacks can reach the Window instance.
    glfwSetWindowUserPointer(m_Handle, this);

#if defined(__APPLE__)
    InstallCocoaLiveResizeRefreshHook(m_Handle);
#endif

    // Scroll callback - GLFW only reports scroll via callback, so we accumulate
    // it into the Input system for per-frame consumption.
    glfwSetScrollCallback(m_Handle,
                          [](GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
                          { Input::AccumulateScroll(static_cast<float>(yoffset)); });
}

void Window::Shutdown()
{
    if (m_Handle)
    {
#if defined(__APPLE__)
        UninstallCocoaLiveResizeRefreshHook(m_Handle);
#endif
        glfwDestroyWindow(m_Handle);
        m_Handle = nullptr;
    }
}

void Window::PollEvents()
{
    glfwPollEvents();
}

NativeWindowHandle Window::GetNativeWindowHandle() const
{
    RTRLAB_ASSERT_MSG(m_Handle != nullptr, "Window handle is null.");

#if defined(_WIN32)
    return CreateWin32NativeWindowHandle(m_Handle);
#elif defined(__APPLE__)
    return CreateCocoaNativeWindowHandle(m_Handle);
#elif defined(__linux__)
    return CreateLinuxNativeWindowHandle(m_Handle);
#else
#error Unsupported platform
#endif
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Handle) != 0;
}

void Window::SetRefreshCallback(RefreshCallback callback)
{
    m_RefreshCallback = std::move(callback);
    glfwSetWindowRefreshCallback(m_Handle,
                                 [](GLFWwindow* w)
                                 {
                                     auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                                     if (self)
                                         self->InvokeRefreshCallback();
                                 });
}

void Window::InvokeRefreshCallback()
{
    if (m_RefreshCallback)
        m_RefreshCallback();
}

void Window::SetEventBus(EventBus* bus)
{
    m_EventBus = bus;
    if (m_EventBus)
        InstallCallbacks();
}

void Window::InstallCallbacks()
{
    // Framebuffer resize - update width/height and publish via EventBus.
    glfwSetFramebufferSizeCallback(m_Handle,
                                   [](GLFWwindow* w, int width, int height)
                                   {
                                       auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                                       if (!self)
                                           return;

                                       self->m_Width = static_cast<uint32_t>(width);
                                       self->m_Height = static_cast<uint32_t>(height);

                                       if (self->m_EventBus)
                                           self->m_EventBus->Publish(WindowResizeEvent{self->m_Width, self->m_Height});
                                   });

    // Window close
    glfwSetWindowCloseCallback(m_Handle,
                               [](GLFWwindow* w)
                               {
                                   auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                                   if (self && self->m_EventBus)
                                       self->m_EventBus->Publish(WindowCloseEvent{});
                               });

    // Keyboard
    glfwSetKeyCallback(m_Handle,
                       [](GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/)
                       {
                           auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                           if (!self || !self->m_EventBus)
                               return;

                           if (action == GLFW_PRESS)
                               self->m_EventBus->Publish(KeyPressedEvent{static_cast<Key::Code>(key), false});
                           else if (action == GLFW_REPEAT)
                               self->m_EventBus->Publish(KeyPressedEvent{static_cast<Key::Code>(key), true});
                           else if (action == GLFW_RELEASE)
                               self->m_EventBus->Publish(KeyReleasedEvent{static_cast<Key::Code>(key)});
                       });

    // Character input (Unicode codepoints for text input)
    glfwSetCharCallback(m_Handle,
                        [](GLFWwindow* w, unsigned int codepoint)
                        {
                            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                            if (self && self->m_EventBus)
                                self->m_EventBus->Publish(CharTypedEvent{codepoint});
                        });

    // Mouse buttons
    glfwSetMouseButtonCallback(
        m_Handle,
        [](GLFWwindow* w, int button, int action, int /*mods*/)
        {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
            if (!self || !self->m_EventBus)
                return;

            if (action == GLFW_PRESS)
                self->m_EventBus->Publish(MouseButtonPressedEvent{static_cast<Mouse::Code>(button)});
            else
                self->m_EventBus->Publish(MouseButtonReleasedEvent{static_cast<Mouse::Code>(button)});
        });

    // Scroll - both feed the Input accumulator AND publish an event.
    glfwSetScrollCallback(m_Handle,
                          [](GLFWwindow* w, double xoffset, double yoffset)
                          {
                              Input::AccumulateScroll(static_cast<float>(yoffset));

                              auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
                              if (self && self->m_EventBus)
                                  self->m_EventBus->Publish(
                                      MouseScrolledEvent{static_cast<float>(xoffset), static_cast<float>(yoffset)});
                          });
}
