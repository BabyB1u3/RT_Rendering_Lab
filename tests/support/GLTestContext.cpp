#include "GlTestContext.h"

#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IRenderCommand.h"
#include "graphics/opengl/GLGraphicsDevice.h"

GlTestContext::GlTestContext()
{
    if (!glfwInit())
        throw std::runtime_error("glfwInit failed");

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    m_Window = glfwCreateWindow(64, 64, "RTRLab Test Context", nullptr, nullptr);
    if (!m_Window)
    {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(m_Window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
        glfwTerminate();
        throw std::runtime_error("gladLoadGLLoader failed");
    }

    SetDevice(CreateRef<GLGraphicsDevice>());
    GetDevice()->GetRenderCommand()->Init();

    m_Initialized = true;
}

GlTestContext::~GlTestContext()
{
    SetDevice(nullptr);

    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    glfwTerminate();
}