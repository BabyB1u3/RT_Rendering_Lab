#include "Window.h"

#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "EventBus.h"
#include "Events.h"
#include "Input.h"
#include "Logger.h"

namespace
{
    bool s_GLFWInitialized = false;

    void GLFWErrorCallback(int error, const char *description)
    {
        LOG_ERROR("[GLFW Error] ({}): {}", error, description);
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

        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW.");

        LOG_INFO("GLFW initialized");
        s_GLFWInitialized = true;
    }

    // macOS caps out at OpenGL 4.1 (Core Profile, forward-compatible).
    // Windows/Linux request 4.6 with a debug context for GL error callbacks.
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    m_Handle = glfwCreateWindow(
        static_cast<int>(props.Width),
        static_cast<int>(props.Height),
        props.Title.c_str(),
        nullptr,
        nullptr);

    if (!m_Handle)
        throw std::runtime_error("Failed to create GLFW window.");

    LOG_INFO("Window created: {}x{} \"{}\"", props.Width, props.Height, props.Title);

    glfwMakeContextCurrent(m_Handle);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

    // Register an OpenGL debug callback to surface driver warnings/errors via our Logger.
    // GL_DEBUG_OUTPUT_SYNCHRONOUS ensures the callback fires on the calling thread,
    // making stack traces useful. Notifications are filtered out to reduce noise.
#ifndef __APPLE__
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
            LOG_ERROR("[GL {}] {} (id={}): {}", srcStr, typeStr, id, message);
        else
            LOG_WARN("[GL {}] {} (id={}): {}", srcStr, typeStr, id, message);
    }, nullptr);
    LOG_INFO("OpenGL debug callback registered");
#endif

    // Store 'this' in the GLFW window so that static callbacks can reach the Window instance.
    glfwSetWindowUserPointer(m_Handle, this);

    // Forward framebuffer resize events to the Application-provided callback.
    glfwSetFramebufferSizeCallback(m_Handle, [](GLFWwindow *window, int width, int height)
                                   {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;

            self->m_Width = static_cast<uint32_t>(width);
            self->m_Height = static_cast<uint32_t>(height);

            if (self->m_ResizeCallback)
                self->m_ResizeCallback(self->m_Width, self->m_Height); });

    // Scroll callback — GLFW only reports scroll via callback, so we accumulate
    // it into the Input system for per-frame consumption.
    glfwSetScrollCallback(m_Handle, [](GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
    {
        Input::AccumulateScroll(static_cast<float>(yoffset));
    });

    SetVSync(props.VSync);

    LOG_INFO("OpenGL Vendor   : {}", reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
    LOG_INFO("OpenGL Renderer : {}", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    LOG_INFO("OpenGL Version  : {}", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
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
    glfwSwapBuffers(m_Handle);
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Handle) != 0;
}

void Window::SetVSync(bool enabled)
{
    glfwSwapInterval(enabled ? 1 : 0);
    m_VSync = enabled;
}

void Window::SetResizeCallback(ResizeCallback callback)
{
    m_ResizeCallback = std::move(callback);
}

void Window::SetEventBus(EventBus* bus)
{
    m_EventBus = bus;
    if (m_EventBus)
        InstallCallbacks();
}

void Window::InstallCallbacks()
{
    // Framebuffer resize — update width/height, fire legacy callback AND EventBus.
    glfwSetFramebufferSizeCallback(m_Handle, [](GLFWwindow* w, int width, int height)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self) return;

        self->m_Width  = static_cast<uint32_t>(width);
        self->m_Height = static_cast<uint32_t>(height);

        if (self->m_ResizeCallback)
            self->m_ResizeCallback(self->m_Width, self->m_Height);

        if (self->m_EventBus)
            self->m_EventBus->Publish(WindowResizeEvent{ self->m_Width, self->m_Height });
    });

    // Window close
    glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow* w)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self && self->m_EventBus)
            self->m_EventBus->Publish(WindowCloseEvent{});
    });

    // Keyboard
    glfwSetKeyCallback(m_Handle, [](GLFWwindow* w, int key, int /*scancode*/,
                                     int action, int /*mods*/)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_EventBus) return;

        if (action == GLFW_PRESS)
            self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), false });
        else if (action == GLFW_REPEAT)
            self->m_EventBus->Publish(KeyPressedEvent{ static_cast<Key::Code>(key), true });
        else if (action == GLFW_RELEASE)
            self->m_EventBus->Publish(KeyReleasedEvent{ static_cast<Key::Code>(key) });
    });

    // Character input (Unicode codepoints for text input)
    glfwSetCharCallback(m_Handle, [](GLFWwindow* w, unsigned int codepoint)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self && self->m_EventBus)
            self->m_EventBus->Publish(CharTypedEvent{ codepoint });
    });

    // Mouse buttons
    glfwSetMouseButtonCallback(m_Handle, [](GLFWwindow* w, int button,
                                             int action, int /*mods*/)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_EventBus) return;

        if (action == GLFW_PRESS)
            self->m_EventBus->Publish(MouseButtonPressedEvent{ static_cast<Mouse::Code>(button) });
        else
            self->m_EventBus->Publish(MouseButtonReleasedEvent{ static_cast<Mouse::Code>(button) });
    });

    // Scroll — both feed the Input accumulator AND publish an event.
    glfwSetScrollCallback(m_Handle, [](GLFWwindow* w, double xoffset, double yoffset)
    {
        Input::AccumulateScroll(static_cast<float>(yoffset));

        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self && self->m_EventBus)
            self->m_EventBus->Publish(MouseScrolledEvent{
                static_cast<float>(xoffset),
                static_cast<float>(yoffset)
            });
    });
}
