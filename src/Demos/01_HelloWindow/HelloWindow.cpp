#include "HelloWindow.h"

#include <imgui.h>

#include "Core/Diagnostics/LogCategories.h"
#include "Core/Diagnostics/LogMacros.h"

HelloWindow::HelloWindow(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void HelloWindow::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "HelloWindow demo attached");
}

void HelloWindow::OnDetach()
{
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::Demo, "HelloWindow demo detached");
}

void HelloWindow::OnRender()
{
}

void HelloWindow::OnImGuiRender()
{
}

void HelloWindow::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}
