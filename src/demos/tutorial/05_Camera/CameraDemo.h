#pragma once

/// @file CameraDemo.h
/// @brief Tutorial 05: WASD free-flight camera with multiple textured cubes.
///
/// Validates: Camera class (view/projection separation), DebugCameraController,
/// InputActionMap, window resize handling, per-draw model matrix update.
/// Reuses UnlitTransformed.slang from Tutorial 04.

#include <cstdint>

#include "core/Base.h"
#include "core/input/InputAction.h"
#include "demos/DemoBase.h"
#include "scene/Camera.h"
#include "scene/DebugCameraController.h"

class IRenderTarget;
class ITexture2D;
class IShader;
class Mesh;

class CameraDemo : public DemoBase
{
public:
    CameraDemo(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void HandleCameraInput(double dt);

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Ref<IRenderTarget> m_BackBuffer;
    Ref<Mesh> m_CubeMesh;
    Ref<ITexture2D> m_Texture;
    Ref<IShader> m_Shader;

    Camera m_Camera;
    DebugCameraController m_CameraController;
    InputActionMap m_InputMap;
};
