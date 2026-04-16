#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Diagnostics/Assert/Assert.h"

void OpenGLCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Early clear-only bring-up path:
    // - uses renderArea directly
    // - assumes today's full-window renderArea usage
    // - must translate from the public RHI coordinate convention to GL's lower-left
    //   origin once partial render areas are exercised
    const Rect2D renderArea = renderingInfo.m_RenderArea;
    glViewport(renderArea.m_X,
               renderArea.m_Y,
               static_cast<GLsizei>(renderArea.m_Width),
               static_cast<GLsizei>(renderArea.m_Height));
    glEnable(GL_SCISSOR_TEST);
    glScissor(renderArea.m_X,
              renderArea.m_Y,
              static_cast<GLsizei>(renderArea.m_Width),
              static_cast<GLsizei>(renderArea.m_Height));

    GLbitfield clearMask = 0;

    if (!renderingInfo.m_ColorAttachments.empty())
    {
        // Early bring-up limitation: only the first color attachment clear is consumed here.
        // Extend this to all color attachments when MRT clear support becomes a real requirement.
        const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
        if (colorAttachment.m_LoadOp == LoadOp::Clear)
        {
            glClearColor(colorAttachment.m_ClearValue.m_R,
                         colorAttachment.m_ClearValue.m_G,
                         colorAttachment.m_ClearValue.m_B,
                         colorAttachment.m_ClearValue.m_A);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (renderingInfo.m_DepthAttachment.m_View != nullptr && renderingInfo.m_DepthAttachment.m_LoadOp == LoadOp::Clear)
    {
        glClearDepth(renderingInfo.m_DepthAttachment.m_ClearValue.m_Depth);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (clearMask != 0)
        glClear(clearMask);
}

void OpenGLCommandList::EndRendering()
{
    ShellCommandListBase::EndRendering();
    glDisable(GL_SCISSOR_TEST);
}

OpenGLSwapchain::OpenGLSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
}

void OpenGLSwapchain::Present(uint32_t imageIndex)
{
    ShellSwapchainBase::Present(imageIndex);

    // v1 relies on the current single-window/single-context invariant:
    // the GLFW current context is expected to belong to this swapchain's window.
    // Revisit this path before adding multi-window or multi-context OpenGL support.
    GLFWwindow* currentContext = glfwGetCurrentContext();
    RTRLAB_ASSERT_MSG(currentContext != nullptr, "OpenGLSwapchain::present requires a current GLFW OpenGL context.");
    glfwSwapBuffers(currentContext);
}

Scope<Swapchain> OpenGLDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    return CreateScope<OpenGLSwapchain>(desc, nativeWindowHandle);
}

CommandList* OpenGLDevice::BeginCommandList()
{
    return &m_CommandList;
}

void OpenGLDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "OpenGLDevice::Submit expects the backend-owned command list returned by BeginCommandList().");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(),
                      "OpenGLDevice::Submit requires EndRendering() before submission.");

    // Early bring-up limitation: Submit currently validates sequencing only.
    // Real Draw submission will need to honor recorded viewport/scissor state and replay
    // Draw commands instead of relying on BeginRendering()-time clear only.
    (void)m_CommandList.GetRenderingInfo();
}

FrameContext* OpenGLDevice::BeginFrame()
{
    return &m_FrameContext;
}
