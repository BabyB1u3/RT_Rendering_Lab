#include "Core/App/Window.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#if defined(GLAB_GLFW_X11_NATIVE)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#endif

#if defined(GLAB_BACKEND_OPENGL)
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#if defined(__APPLE__)
#include "Core/App/CocoaNativeWindow.h"
#endif
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Event/EventBus.h"
#include "Core/Event/Events.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Input/Input.h"

namespace
{
    bool s_GLFWInitialized = false;

    void GLFWErrorCallback(int error, const char *description)
    {
        LOG_ERROR_CAT(LogCategory::Window, "[GLFW Error] ({}): {}", error, description);
    }
}

Window::Window(const WindowProps &props)
{
    Init(props);
}

Window::~Window()
{
    Shutdown();
}

void Window::Init(const WindowProps &props)
{
    m_Width = props.Width;
    m_Height = props.Height;
    m_VSync = props.VSync;

    if (!s_GLFWInitialized)
    {
        glfwSetErrorCallback(GLFWErrorCallback);

        RTRLAB_ASSERT_MSG(glfwInit(), "Failed to initialize GLFW.");

        LOG_INFO_CAT(LogCategory::Window, "GLFW initialized");
        s_GLFWInitialized = true;
    }

#if defined(GLAB_BACKEND_OPENGL)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#elif defined(GLAB_BACKEND_METAL) || defined(GLAB_BACKEND_VULKAN)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
#error Unsupported graphics backend
#endif

    m_Handle = glfwCreateWindow(
        static_cast<int>(props.Width),
        static_cast<int>(props.Height),
        props.Title.c_str(),
        nullptr,
        nullptr);

    RTRLAB_ASSERT_MSG(m_Handle, "Failed to create GLFW window.");

    // On HiDPI/Retina displays the framebuffer size (physical pixels) differs from
    // the window size (logical points). Always store physical pixel dimensions so that
    // viewport setup is correct from the very first frame.
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(m_Handle, &fbWidth, &fbHeight);
    m_Width = static_cast<uint32_t>(fbWidth);
    m_Height = static_cast<uint32_t>(fbHeight);

    LOG_INFO_CAT(LogCategory::Window,
                 "Window created: {}x{} (framebuffer {}x{}) \"{}\"",
                 props.Width, props.Height, m_Width, m_Height, props.Title);

#if defined(GLAB_BACKEND_OPENGL)
    glfwMakeContextCurrent(m_Handle);

    RTRLAB_ASSERT_MSG(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)),
                      "Failed to initialize GLAD.");

    // Register an OpenGL debug callback to surface driver warnings/errors via our Logger.
    // GL_DEBUG_OUTPUT_SYNCHRONOUS ensures the callback fires on the calling thread,
    // making stack traces useful. Notifications are filtered out to reduce noise.
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
                              GLenum severity, GLsizei /*length*/,
                              const GLchar *message, const void * /*userParam*/)
                           {
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

        const char *srcStr = [source]() -> const char * {
            switch (source) {
                case GL_DEBUG_SOURCE_API:             return "API";
                case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "Window System";
                case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
                case GL_DEBUG_SOURCE_THIRD_PARTY:     return "Third Party";
                case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
                default:                              return "Other";
            }
        }();

        const char *typeStr = [type]() -> const char * {
            switch (type) {
                case GL_DEBUG_TYPE_ERROR:               return "Error";
                case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated";
                case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "Undefined Behavior";
                case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
                case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
                default:                                return "Other";
            }
        }();

        if (severity == GL_DEBUG_SEVERITY_HIGH)
            LOG_ERROR_CAT(LogCategory::Window, "[GL {}] {} (id={}): {}", srcStr, typeStr, id, message);
        else
            LOG_WARN_CAT(LogCategory::Window, "[GL {}] {} (id={}): {}", srcStr, typeStr, id, message); },
                           nullptr);
    LOG_INFO_CAT(LogCategory::Window, "OpenGL debug callback registered");
#endif

    // Store 'this' in the GLFW window so that static callbacks can reach the Window instance.
    glfwSetWindowUserPointer(m_Handle, this);

    // Scroll callback - GLFW only reports scroll via callback, so we accumulate
    // it into the Input system for per-frame consumption.
    glfwSetScrollCallback(m_Handle, [](GLFWwindow * /*window*/, double /*xoffset*/, double yoffset)
                          { Input::AccumulateScroll(static_cast<float>(yoffset)); });

    SetVSync(props.VSync);

#if defined(GLAB_BACKEND_OPENGL)
    LOG_INFO_CAT(LogCategory::Window, "OpenGL Vendor   : {}", reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
    LOG_INFO_CAT(LogCategory::Window, "OpenGL Renderer : {}", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    LOG_INFO_CAT(LogCategory::Window, "OpenGL Version  : {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
#endif
}

void Window::Shutdown()
{
    if (m_Handle)
    {
        glfwDestroyWindow(m_Handle);
        m_Handle = nullptr;
    }
}

void Window::PollEvents()
{
    glfwPollEvents();
}

void Window::SwapBuffers()
{
#if defined(GLAB_BACKEND_OPENGL)
    glfwSwapBuffers(m_Handle);
#endif
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Handle) != 0;
}

NativeWindowHandle Window::GetNativeWindowHandle() const
{
    RTRLAB_ASSERT_MSG(m_Handle != nullptr, "Window handle is null.");

#if defined(_WIN32)
    NativeWindowHandle nativeWindowHandle{};
    nativeWindowHandle.system = NativeWindowSystem::Win32;
    nativeWindowHandle.window = reinterpret_cast<uintptr_t>(glfwGetWin32Window(m_Handle));
    return nativeWindowHandle;
#elif defined(__APPLE__)
    return CreateCocoaNativeWindowHandle(m_Handle);
#elif defined(__linux__)
    NativeWindowHandle nativeWindowHandle{};
    const int platform = glfwGetPlatform();

#if defined(GLAB_GLFW_WAYLAND_NATIVE)
    if (platform == GLFW_PLATFORM_WAYLAND)
    {
        nativeWindowHandle.system = NativeWindowSystem::Wayland;
        nativeWindowHandle.window = reinterpret_cast<uintptr_t>(glfwGetWaylandWindow(m_Handle));
        nativeWindowHandle.display = glfwGetWaylandDisplay();
        return nativeWindowHandle;
    }
#endif

#if defined(GLAB_GLFW_X11_NATIVE)
    if (platform == GLFW_PLATFORM_X11)
    {
        nativeWindowHandle.system = NativeWindowSystem::Xlib;
        nativeWindowHandle.window = static_cast<uintptr_t>(glfwGetX11Window(m_Handle));
        nativeWindowHandle.display = glfwGetX11Display();
        return nativeWindowHandle;
    }
#endif

    RTRLAB_ASSERT_MSG(false, "Unsupported GLFW native platform.");
    return nativeWindowHandle;
#else
#error Unsupported platform
#endif
}

void Window::SetVSync(bool enabled)
{
#if defined(GLAB_BACKEND_OPENGL)
    glfwSwapInterval(enabled ? 1 : 0);
#endif
    m_VSync = enabled;
}

void Window::SetRefreshCallback(RefreshCallback callback)
{
    m_RefreshCallback = std::move(callback);
    glfwSetWindowRefreshCallback(m_Handle, [](GLFWwindow *w)
                                 {
        auto *self = static_cast<Window *>(glfwGetWindowUserPointer(w));
        if (self && self->m_RefreshCallback)
            self->m_RefreshCallback(); });
}

void Window::SetEventBus(EventBus *bus)
{
    m_EventBus = bus;
    if (m_EventBus)
        InstallCallbacks();
}

void Window::InstallCallbacks()
{
    // Framebuffer resize - update width/height and publish via EventBus.
    glfwSetFramebufferSizeCallback(m_Handle, [](GLFWwindow *w, int width, int height)
                                   {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self) return;

        self->m_Width  = static_cast<uint32_t>(width);
        self->m_Height = static_cast<uint32_t>(height);

        if (self->m_EventBus)
            self->m_EventBus->Publish(WindowResizeEvent{ self->m_Width, self->m_Height }); });

    // Window close
    glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow *w)
                               {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self && self->m_EventBus)
            self->m_EventBus->Publish(WindowCloseEvent{}); });

    // Keyboard
    glfwSetKeyCallback(m_Handle, [](GLFWwindow *w, int key, int /*scancode*/,
                                    int action, int /*mods*/)
                       {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_EventBus) return;

        if (action == GLFW_PRESS)
            self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), false });
        else if (action == GLFW_REPEAT)
            self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), true });
        else if (action == GLFW_RELEASE)
            self->m_EventBus->Publish(KeyReleasedEvent{ static_cast<Key::Code>(key) }); });

    // Character input (Unicode codepoints for text input)
    glfwSetCharCallback(m_Handle, [](GLFWwindow *w, unsigned int codepoint)
                        {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self && self->m_EventBus)
            self->m_EventBus->Publish(CharTypedEvent{ codepoint }); });

    // Mouse buttons
    glfwSetMouseButtonCallback(m_Handle, [](GLFWwindow *w, int button,
                                            int action, int /*mods*/)
                               {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_EventBus) return;

        if (action == GLFW_PRESS)
            self->m_EventBus->Publish(MouseButtonPressedEvent{ static_cast<Mouse::Code>(button) });
        else
            self->m_EventBus->Publish(MouseButtonReleasedEvent{ static_cast<Mouse::Code>(button) }); });

    // Scroll - both feed the Input accumulator AND publish an event.
    glfwSetScrollCallback(m_Handle, [](GLFWwindow *w, double xoffset, double yoffset)
                          {
        Input::AccumulateScroll(static_cast<float>(yoffset));

        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self && self->m_EventBus)
            self->m_EventBus->Publish(MouseScrolledEvent{
                static_cast<float>(xoffset),
                static_cast<float>(yoffset)
            }); });
}
