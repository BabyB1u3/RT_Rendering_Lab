#include "ClearColorDemo.h"

#include <imgui.h>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

ClearColorDemo::ClearColorDemo(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height) {}

void ClearColorDemo::OnAttach()
{
    LOG_INFO_CAT(LogCategory::k_Demo, "ClearColorDemo demo attached");
}

void ClearColorDemo::OnDetach()
{
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::k_Demo, "ClearColorDemo demo detached");
}

void ClearColorDemo::OnRender()
{
    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "ClearColorDemo requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "ClearColorDemo requires the application to expose the active swapchain backbuffer.");

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

void ClearColorDemo::OnImGuiRender()
{
    ImGui::Begin("Clear Color");
    ImGui::ColorEdit3("Clear Color", m_ClearColor.data());
    ImGui::End();
}

void ClearColorDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}
