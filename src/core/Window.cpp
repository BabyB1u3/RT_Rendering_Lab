#include "Window.h"

#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

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

    glfwSetWindowUserPointer(m_Handle, this);

    glfwSetFramebufferSizeCallback(m_Handle, [](GLFWwindow *window, int width, int height)
                                   {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (!self)
                return;

            self->m_Width = static_cast<uint32_t>(width);
            self->m_Height = static_cast<uint32_t>(height);

            if (self->m_ResizeCallback)
                self->m_ResizeCallback(self->m_Width, self->m_Height); });

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
