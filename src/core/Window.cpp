#include "core/Window.h"

#include <stdexcept>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace
{
    bool s_GLFWInitialized = false;

    void GLFWErrorCallback(int error, const char *description)
    {
        std::cerr << "[GLFW Error] (" << error << "): " << description << '\n';
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

        s_GLFWInitialized = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Handle = glfwCreateWindow(
        static_cast<int>(props.Width),
        static_cast<int>(props.Height),
        props.Title.c_str(),
        nullptr,
        nullptr);

    if (!m_Handle)
        throw std::runtime_error("Failed to create GLFW window.");

    glfwMakeContextCurrent(m_Handle);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

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

    std::cout << "OpenGL Vendor   : " << glGetString(GL_VENDOR) << '\n';
    std::cout << "OpenGL Renderer : " << glGetString(GL_RENDERER) << '\n';
    std::cout << "OpenGL Version  : " << glGetString(GL_VERSION) << '\n';
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
