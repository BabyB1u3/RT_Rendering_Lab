#pragma once

/// @file ClearScreen.h
/// @brief Tutorial 01: clear the screen to a user-chosen color.
///
/// Validates the most basic RHI path: window creation, back buffer render
/// target, BeginFrame/EndFrame, BeginRenderPass/EndRenderPass, and present.
/// No shaders, no geometry - just a solid color fill controlled by ImGui.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "demos/DemoBase.h"

class IRenderTarget;

class ClearScreen : public DemoBase
{
public:
    ClearScreen(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Ref<IRenderTarget> m_BackBuffer;

    glm::vec3 m_ClearColor{0.2f, 0.3f, 0.3f};
};
