#pragma once

/// @file Triangle.h
/// @brief Tutorial 02: draw a colored triangle from hand-built NDC vertex data.
///
/// Validates the minimum draw path: IVertexBuffer, IVertexArray, BufferLayout,
/// IShader bind + DrawArrays. No MeshFactory, no index buffer, no MVP - just
/// three vertices in NDC fed through FlatColor.slang.

#include <cstdint>

#include "core/Base.h"
#include "demos/DemoBase.h"

class IRenderTarget;
class IVertexArray;
class IVertexBuffer;
class IShader;

class Triangle : public DemoBase
{
public:
    Triangle(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Ref<IRenderTarget> m_BackBuffer;
    Ref<IVertexArray> m_VAO;
    Ref<IVertexBuffer> m_VBO;
    Ref<IShader> m_Shader;
};
