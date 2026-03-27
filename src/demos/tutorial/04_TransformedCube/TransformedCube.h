#pragma once

/// @file TransformedCube.h
/// @brief Tutorial 04: a continuously rotating textured cube with perspective projection.
///
/// Validates: MVP matrix uniform upload (SetUniformBlock), depth testing
/// (PipelineState.DepthTestEnabled), 3D geometry via MeshFactory::CreateCube().
/// Uses UnlitTransformed.slang - position + normal + UV with u_MVP uniform.

#include <cstdint>

#include "core/Base.h"
#include "demos/DemoBase.h"

class IRenderTarget;
class ITexture2D;
class IShader;
class Mesh;

class TransformedCube : public DemoBase
{
public:
    TransformedCube(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Ref<IRenderTarget> m_BackBuffer;
    Ref<Mesh> m_CubeMesh;
    Ref<ITexture2D> m_Texture;
    Ref<IShader> m_Shader;

    float m_RotationY = 0.0f;
    float m_RotationX = 0.0f;
    float m_RotationSpeed = 45.0f; // degrees per second
};
