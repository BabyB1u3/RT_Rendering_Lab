#include "HelloWindow.h"

#include <imgui.h>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

HelloWindow::HelloWindow(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height) {}

void HelloWindow::OnAttach()
{
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloWindow demo attached");
}

void HelloWindow::OnDetach()
{
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloWindow demo detached");
}

void HelloWindow::OnRender()
{
    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "HelloWindow requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "HelloWindow requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    resourceStateTracker.Transition(swapchainImage, TextureState::RenderTarget);
    resourceStateTracker.FlushBarriers(commandList);

    ColorAttachmentInfo colorAttachment;
    colorAttachment.m_View = swapchainImageView;
    colorAttachment.m_LoadOp = LoadOp::Clear;
    colorAttachment.m_StoreOp = StoreOp::Store;
    colorAttachment.m_ClearValue = {m_ClearColor.x(), m_ClearColor.y(), m_ClearColor.z(), 1.0f};

    RenderingInfo renderingInfo;
    renderingInfo.m_ColorAttachments = {colorAttachment};
    renderingInfo.m_RenderArea = {0, 0, m_ViewportWidth, m_ViewportHeight};

    commandList->BeginRendering(renderingInfo);
    commandList->EndRendering();
}

void HelloWindow::OnImGuiRender()
{
    ImGui::Begin("Hello Window");
    ImGui::ColorEdit3("Clear Color", m_ClearColor.data());
    ImGui::End();
}

void HelloWindow::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}
