#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Diagnostics/Assert/Assert.h"

OpenGLSwapchain::OpenGLSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
}

void OpenGLSwapchain::present(uint32_t imageIndex)
{
    ShellSwapchainBase::present(imageIndex);

    // v1 relies on the current single-window/single-context invariant:
    // the GLFW current context is expected to belong to this swapchain's window.
    // Revisit this path before adding multi-window or multi-context OpenGL support.
    GLFWwindow *currentContext = glfwGetCurrentContext();
    RTRLAB_ASSERT_MSG(currentContext != nullptr,
                      "OpenGLSwapchain::present requires a current GLFW OpenGL context.");
    glfwSwapBuffers(currentContext);
}

Scope<Swapchain> OpenGLDevice::createSwapchain(const SwapchainDesc &desc, const NativeWindowHandle &nativeWindowHandle)
{
    return CreateScope<OpenGLSwapchain>(desc, nativeWindowHandle);
}

CommandList *OpenGLDevice::beginCommandList()
{
    return &m_CommandList;
}

void OpenGLDevice::submit(CommandList *commandList)
{
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "OpenGLDevice::submit expects the backend-owned command list returned by beginCommandList().");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(),
                      "OpenGLDevice::submit requires endRendering() before submission.");

    const RenderingInfo &renderingInfo = m_CommandList.GetRenderingInfo();
    if (renderingInfo.colorAttachments.empty() && renderingInfo.depthAttachment.view == nullptr)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const Rect2D renderArea = renderingInfo.renderArea;
    glViewport(renderArea.x,
               renderArea.y,
               static_cast<GLsizei>(renderArea.width),
               static_cast<GLsizei>(renderArea.height));
    glEnable(GL_SCISSOR_TEST);
    glScissor(renderArea.x,
              renderArea.y,
              static_cast<GLsizei>(renderArea.width),
              static_cast<GLsizei>(renderArea.height));

    GLbitfield clearMask = 0;

    if (!renderingInfo.colorAttachments.empty())
    {
        const ColorAttachmentInfo &colorAttachment = renderingInfo.colorAttachments.front();
        if (colorAttachment.loadOp == LoadOp::Clear)
        {
            glClearColor(colorAttachment.clearValue.r,
                         colorAttachment.clearValue.g,
                         colorAttachment.clearValue.b,
                         colorAttachment.clearValue.a);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (renderingInfo.depthAttachment.view != nullptr && renderingInfo.depthAttachment.loadOp == LoadOp::Clear)
    {
        glClearDepth(renderingInfo.depthAttachment.clearValue.depth);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (clearMask != 0)
        glClear(clearMask);

    glDisable(GL_SCISSOR_TEST);
}

FrameContext *OpenGLDevice::beginFrame()
{
    return &m_FrameContext;
}
