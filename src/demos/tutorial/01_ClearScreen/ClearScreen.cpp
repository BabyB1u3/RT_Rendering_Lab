#include "ClearScreen.h"

#include <imgui.h>

#include "core/Logger.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"

ClearScreen::ClearScreen(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void ClearScreen::OnAttach()
{
    LOG_INFO("ClearScreen demo attached");
    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);
}

void ClearScreen::OnDetach()
{
    m_BackBuffer.reset();
    LOG_INFO("ClearScreen demo detached");
}

void ClearScreen::OnRender()
{
    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::Clear;
    desc.ClearColor = {m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, 1.0f};

    RenderCommand::BeginRenderPass(m_BackBuffer, desc);
    RenderCommand::SetViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    RenderCommand::EndRenderPass();
}

void ClearScreen::OnImGuiRender()
{
    ImGui::Begin("01 - Clear Screen");
    ImGui::ColorEdit3("Clear Color", &m_ClearColor.r);
    ImGui::End();
}

void ClearScreen::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
}
