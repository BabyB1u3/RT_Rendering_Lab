#pragma once

/// @file ShadowMapping.h
/// @brief Shadow-mapping demo: cubes on a floor lit by a single directional light.
///
/// Demonstrates the full 3-pass rendering pipeline (ShadowPass → ForwardPass →
/// TexturePreviewPass). The ImGui panel exposes output mode (final color vs.
/// shadow map debug view), light direction/color/intensity, and light projection
/// parameters (distance, ortho size, near/far).

#include <cstdint>
#include <memory>

#include "core/input/InputAction.h"
#include "demos/DemoBase.h"
#include "renderer/SceneRenderer.h"
#include "scene/Camera.h"
#include "scene/DebugCameraController.h"
#include "scene/SceneData.h"

class Mesh;
class Material;
class ITexture2D;

class ShadowMapping : public DemoBase
{
public:
    ShadowMapping(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void BuildScene();
    void HandleCameraInput(double dt);

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Camera m_Camera;
    DebugCameraController m_CameraController;

    Scope<SceneRenderer> m_Renderer;
    SceneData m_Scene;

    Ref<Mesh> m_CubeMesh;
    Ref<Mesh> m_PlaneMesh;

    Ref<Material> m_DefaultMaterial;
    Ref<ITexture2D> m_WhiteTexture;

    InputActionMap m_InputMap;
    SceneRendererOutput m_OutputMode = SceneRendererOutput::FinalColor;
};
