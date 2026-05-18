#pragma once

/// @file MultiObjectTexturedSceneDemo.h
/// @brief Tutorial 07: draw a small multi-object textured scene.

#include <cstdint>

#include "Demos/DemoBase.h"

class MultiObjectTexturedSceneDemo : public DemoBase
{
public:
    MultiObjectTexturedSceneDemo(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
    float m_ElapsedSeconds = 0.0f;
};
