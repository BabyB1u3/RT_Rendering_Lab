#pragma once

/// @file ForwardPass.h
/// @brief Main forward-shading pass: Blinn-Phong lighting + PCF shadow sampling.
///
/// For each RenderItem in the scene:
///   1. Upload model matrix + normal matrix
///   2. Upload material properties (albedo, specular, ambient) via Material::UploadToShader()
///   3. Sample the shadow map from ShadowPass to compute shadow factor
///   4. Shade with Blinn-Phong (ambient + diffuse + specular)
///
/// The pass can render to either an off-screen Framebuffer (for later compositing
/// by TexturePreviewPass) or directly to the back buffer.
///
/// A 1x1 white fallback texture is used when no shadow map is available,
/// ensuring that sampling produces depth=1.0 → no shadow.

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "RenderPass.h"

class IFramebuffer;
class IRenderTarget;
class IShader;
class ITexture2D;
class IUniformBuffer;
class ShaderUniformBlockLayout;

class ForwardPass : public RenderPass
{
public:
    ForwardPass(uint32_t width, uint32_t height, bool renderToTarget,
                const glm::vec4 &clearColor = {0.1f, 0.1f, 0.12f, 1.0f});

    void Resize(unsigned int width, unsigned int height) override;
    void Execute(const RenderContext &ctx) override;

    Ref<IFramebuffer> GetFramebuffer() const { return m_Framebuffer; }

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    bool m_RenderToTarget = true;

    Ref<IFramebuffer> m_Framebuffer;
    Ref<IRenderTarget> m_RenderTarget; ///< Cached (P5)
    glm::vec4 m_ClearColor = {0.1f, 0.1f, 0.12f, 1.0f};

    Ref<IShader> m_Shader;
    const ShaderUniformBlockLayout *m_UniformBlockLayout = nullptr;
    Ref<IUniformBuffer> m_UniformBuffer;
    Ref<ITexture2D> m_FallbackShadowMap;
};
