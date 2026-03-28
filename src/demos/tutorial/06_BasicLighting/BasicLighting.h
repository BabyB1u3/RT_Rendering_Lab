#pragma once

/// @file BasicLighting.h
/// @brief Tutorial 06: Blinn-Phong lighting with a single directional light.
///
/// Validates: normal data passing, lighting calculation (ambient + diffuse +
/// specular), multiple uniform uploads via logical BindUniformBuffer.
/// Uses BasicLit.slang — Phong lighting with ViewProjection, Model,
/// NormalMatrix, light params, and material params.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "core/input/InputAction.h"
#include "demos/DemoBase.h"
#include "scene/Camera.h"
#include "scene/DebugCameraController.h"

class IRenderTarget;
class IShader;
class IUniformBuffer;
class Mesh;
class ShaderUniformBlockLayout;

class BasicLighting : public DemoBase
{
public:
    BasicLighting(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void HandleCameraInput(double dt);
    void DrawCube(const glm::mat4 &vp, const glm::mat4 &model,
                  const glm::vec3 &albedo, float specPower, float ambient);

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Ref<IRenderTarget> m_BackBuffer;
    Ref<Mesh> m_CubeMesh;
    Ref<IShader> m_Shader;
    const ShaderUniformBlockLayout *m_UniformBlockLayout = nullptr;
    // Reused for multiple draws in one pass. Backend BindUniformBuffer()
    // implementations must preserve per-draw contents even when this buffer is
    // updated again before the command buffer finishes executing.
    Ref<IUniformBuffer> m_UniformBuffer;

    Camera m_Camera;
    DebugCameraController m_CameraController;
    InputActionMap m_InputMap;

    // Light parameters (editable via ImGui)
    glm::vec3 m_LightDirection{-0.5f, -1.0f, -0.3f};
    glm::vec3 m_LightColor{1.0f, 1.0f, 1.0f};
    float m_LightIntensity = 1.0f;
};
