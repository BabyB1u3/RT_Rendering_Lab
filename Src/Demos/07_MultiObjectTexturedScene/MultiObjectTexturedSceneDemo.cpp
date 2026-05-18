#include "MultiObjectTexturedSceneDemo.h"

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

MultiObjectTexturedSceneDemo::MultiObjectTexturedSceneDemo(uint32_t width, uint32_t height)
    : m_ViewportWidth(width), m_ViewportHeight(height)
{
}

void MultiObjectTexturedSceneDemo::OnAttach()
{
    LOG_INFO_CAT(LogCategory::k_Demo, "MultiObjectTexturedScene demo attached");
}

void MultiObjectTexturedSceneDemo::OnDetach()
{
    LOG_INFO_CAT(LogCategory::k_Demo, "MultiObjectTexturedScene demo detached");
}

void MultiObjectTexturedSceneDemo::OnUpdate(double dt)
{
    m_ElapsedSeconds += static_cast<float>(dt);
}

void MultiObjectTexturedSceneDemo::OnRender() {}

void MultiObjectTexturedSceneDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}
