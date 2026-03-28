#pragma once

/// @file TexturedQuad.h
/// @brief Tutorial 03: draw a textured quad using indexed drawing.
///
/// Validates: IIndexBuffer (indexed draw), ITexture2D creation and sampling,
/// CreateTexture2DFromFile, logical shader texture binding.
/// Uses UnlitTextured.slang - position + UV, single texture sample, no MVP.

#include <cstdint>

#include "core/Base.h"
#include "demos/DemoBase.h"

class IRenderTarget;
class IVertexArray;
class IVertexBuffer;
class IIndexBuffer;
class ITexture2D;
class IShader;

class TexturedQuad : public DemoBase
{
public:
    TexturedQuad(uint32_t width, uint32_t height);

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
    Ref<IIndexBuffer> m_IBO;
    Ref<ITexture2D> m_Texture;
    Ref<IShader> m_Shader;
};
