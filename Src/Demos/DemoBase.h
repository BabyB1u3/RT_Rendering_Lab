#pragma once

/// @file DemoBase.h
/// @brief Abstract base class for all demos.
///
/// Mirrors the Layer lifecycle (OnAttach/OnDetach/OnUpdate/OnRender/OnImGuiRender/OnResize)
/// so that LabLayer can forward its callbacks to the active demo transparently.
/// Each demo owns its own Camera, SceneRenderer, SceneData, and meshes/materials.

#include <cstdint>

class DemoBase
{
public:
    virtual ~DemoBase() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}

    virtual void OnUpdate(double) {}
    virtual void OnRender() {}
    virtual void OnImGuiRender() {}
    virtual void OnResize(uint32_t, uint32_t) {}
};