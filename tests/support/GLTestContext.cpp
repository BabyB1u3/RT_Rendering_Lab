#include "GLTestContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IRenderCommand.h"
#include "graphics/opengl/GLGraphicsDevice.h"

GlTestContext::GlTestContext()
{
    RTRLAB_ASSERT_MSG(glfwInit(), "GLTestContext: glfwInit failed");

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
        LOG_ERROR_CAT(LogCategory::Window, "GLTestContext: glfwCreateWindow failed");
        glfwTerminate();
        RTRLAB_ASSERT_MSG(m_Window != nullptr, "GLTestContext: glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(m_Window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        LOG_ERROR_CAT(LogCategory::Graphics, "GLTestContext: gladLoadGLLoader failed");
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
        glfwTerminate();
        RTRLAB_ASSERT_MSG(false, "GLTestContext: gladLoadGLLoader failed");
    }

    FileSystem::Init();

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
