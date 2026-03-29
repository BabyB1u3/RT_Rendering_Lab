#include "HelloWindow.h"

#include <imgui.h>

#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"

HelloWindow::HelloWindow(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void HelloWindow::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "HelloWindow demo attached");
    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);
}

void HelloWindow::OnDetach()
{
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::Demo, "HelloWindow demo detached");
}

void HelloWindow::OnRender()
{
    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::Clear;
    desc.ClearColor = {m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, 1.0f};

    RenderCommand::BeginRenderPass(m_BackBuffer, desc);
    RenderCommand::SetViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    RenderCommand::EndRenderPass();
}

void HelloWindow::OnImGuiRender()
{
    ImGui::Begin("01 - Hello Window");
    ImGui::ColorEdit3("Clear Color", &m_ClearColor.r);
    ImGui::End();
}

void HelloWindow::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
}
