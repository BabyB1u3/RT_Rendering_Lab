#pragma once

/// @file TexturePreviewPass.h
/// @brief Final compositing pass - blits a texture to the currently bound framebuffer.
///
/// Draws a fullscreen quad textured with the selected output:
///   - FinalColor mode: scene color from ForwardPass
///   - ShadowMap mode:  depth texture from ShadowPass (linearized to grayscale)
///
/// This pass does not own a framebuffer - it renders to whatever is currently
/// bound (typically the default back buffer).

#include <memory>

#include "core/Base.h"
#include "RenderPass.h"

class IShader;
class Mesh;
class ITexture2D;
class IUniformBuffer;
class ShaderUniformBlockLayout;

class TexturePreviewPass : public RenderPass
{
public:
    TexturePreviewPass();

    void Resize(unsigned int width, unsigned int height) override;
    void Execute(const RenderContext &ctx) override;

private:
    Ref<IShader> m_Shader;
    const ShaderUniformBlockLayout *m_UniformBlockLayout = nullptr;
    Ref<IUniformBuffer> m_UniformBuffer;
    Ref<Mesh> m_FullscreenQuad;
};
