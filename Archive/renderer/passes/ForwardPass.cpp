#include "renderer/passes/ForwardPass.h"

#include <memory>

#include <glm/glm.hpp>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/RenderCommand.h"
#include "graphics/Texture.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"
#include "graphics/interfaces/IUniformBuffer.h"
#include "renderer/RenderContext.h"
#include "renderer/RenderItem.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

namespace
{
    constexpr ShaderBindingPoint kForwardPerPassBinding = ShaderBindingPoints::PerFrame;
    constexpr ShaderBindingPoint kForwardShadowTextureBinding = ShaderBindingPoints::ShadowMap;
    constexpr ShaderBindingPoint kForwardPerMaterialBinding = ShaderBindingPoints::PerMaterial;
    constexpr ShaderBindingPoint kForwardAlbedoTextureBinding = ShaderBindingPoints::MaterialAlbedoMap;
    constexpr ShaderBindingPoint kForwardPerDrawBinding = ShaderBindingPoints::PerDraw;
}

ForwardPass::ForwardPass(uint32_t width, uint32_t height, bool renderToTarget,
                         const glm::vec4 &clearColor)
    : m_Width(width), m_Height(height), m_RenderToTarget(renderToTarget), m_ClearColor(clearColor)
{
    if (m_RenderToTarget)
    {
        FramebufferSpecification fbSpec;
        fbSpec.Width = width;
        fbSpec.Height = height;
        fbSpec.Attachments = {
            {TextureFormat::RGBA8},
            {TextureFormat::Depth24Stencil8}};

        m_Framebuffer = GetDevice()->CreateFramebuffer(fbSpec);
        m_RenderTarget = GetDevice()->CreateRenderTargetFromFramebuffer(m_Framebuffer);
    }

    m_Shader = GetDevice()->CreateShader("ForwardLit");
    RTRLAB_ASSERT_MSG(m_Shader, "ForwardPass failed to create ForwardLit shader");
    m_PerPassLayout = m_Shader->GetUniformBlockLayout(kForwardPerPassBinding);
    m_PerMaterialLayout = m_Shader->GetUniformBlockLayout(kForwardPerMaterialBinding);
    m_PerDrawLayout = m_Shader->GetUniformBlockLayout(kForwardPerDrawBinding);
    RTRLAB_ASSERT_MSG(m_PerPassLayout,
                      "ForwardPass: shader must provide reflected layout for logical binding {0, 0}.");
    RTRLAB_ASSERT_MSG(m_PerMaterialLayout,
                      "ForwardPass: shader must provide reflected layout for logical binding {1, 0}.");
    RTRLAB_ASSERT_MSG(m_PerDrawLayout,
                      "ForwardPass: shader must provide reflected layout for logical binding {2, 0}.");

    m_PerPassBuffer = GetDevice()->CreateUniformBuffer(m_PerPassLayout->GetSize());
    m_PerMaterialBuffer = GetDevice()->CreateUniformBuffer(m_PerMaterialLayout->GetSize());
    m_PerDrawBuffer = GetDevice()->CreateUniformBuffer(m_PerDrawLayout->GetSize());
    RTRLAB_ASSERT_MSG(m_PerPassBuffer, "ForwardPass failed to create uniform buffer for logical binding {0, 0}");
    RTRLAB_ASSERT_MSG(m_PerMaterialBuffer, "ForwardPass failed to create uniform buffer for logical binding {1, 0}");
    RTRLAB_ASSERT_MSG(m_PerDrawBuffer, "ForwardPass failed to create uniform buffer for logical binding {2, 0}");

    // 1x1 white fallback texture for when no shadow map is provided.
    // Sampling r = 1.0 means currentDepth - bias > 1.0 is always false -> no shadow.
    TextureSpecification fallbackSpec;
    fallbackSpec.Width = 1;
    fallbackSpec.Height = 1;
    fallbackSpec.Format = TextureFormat::RGBA8;
    m_FallbackShadowMap = GetDevice()->CreateTexture2D(fallbackSpec);
    RTRLAB_ASSERT_MSG(m_FallbackShadowMap, "ForwardPass failed to create fallback shadow map");
    const uint32_t white = 0xFFFFFFFFu;
    m_FallbackShadowMap->SetData(&white);
}

void ForwardPass::Resize(unsigned int width, unsigned int height)
{
    if (width == 0 || height == 0)
        return;

    m_Width = width;
    m_Height = height;

    if (m_Framebuffer)
        m_Framebuffer->Resize(width, height);
}

void ForwardPass::Execute(const RenderContext &ctx)
{
    RTRLAB_ASSERT_MSG(m_Shader, "ForwardPass shader is null");

    // Determine render target: own FBO or back buffer from context
    auto target = m_RenderToTarget
                      ? m_RenderTarget
                      : ctx.Resources.BackBuffer;

    // P2: Explicit render pass descriptor - clear color + depth
    RenderPassDescriptor rpDesc;
    rpDesc.ColorLoadAction = LoadAction::Clear;
    rpDesc.ColorStoreAction = StoreAction::Store;
    rpDesc.ClearColor = m_ClearColor;
    rpDesc.DepthLoadAction = LoadAction::Clear;
    rpDesc.DepthStoreAction = StoreAction::Store;
    rpDesc.ClearDepth = 1.0f;

    RenderCommand::BeginRenderPass(target, rpDesc);

    // P3: Pipeline state - opaque geometry, back-face culling
    PipelineState pso;
    pso.DepthTestEnabled = true;
    pso.DepthWriteEnabled = true;
    pso.BlendEnabled = false;
    pso.CullFaceEnabled = true;
    pso.CullFront = false;
    RenderCommand::SetPipelineState(pso);

    RenderCommand::SetViewport(0, 0, target->GetWidth(), target->GetHeight());

    const auto &camera = ctx.View.Camera;
    const auto &scene = ctx.View.Scene;

    m_Shader->Bind();
    RTRLAB_ASSERT_MSG(m_PerPassLayout, "ForwardPass per-pass layout is null");
    RTRLAB_ASSERT_MSG(m_PerMaterialLayout, "ForwardPass per-material layout is null");
    RTRLAB_ASSERT_MSG(m_PerDrawLayout, "ForwardPass per-draw layout is null");
    RTRLAB_ASSERT_MSG(m_PerPassBuffer, "ForwardPass per-pass buffer is null");
    RTRLAB_ASSERT_MSG(m_PerMaterialBuffer, "ForwardPass per-material buffer is null");
    RTRLAB_ASSERT_MSG(m_PerDrawBuffer, "ForwardPass per-draw buffer is null");

    const auto &shadow = ctx.Resources.ShadowMap ? ctx.Resources.ShadowMap : m_FallbackShadowMap;
    m_Shader->BindTexture(kForwardShadowTextureBinding, shadow);
    const glm::vec2 shadowMapTexelSize = {
        1.0f / static_cast<float>(shadow->GetWidth()),
        1.0f / static_cast<float>(shadow->GetHeight())};

    PackedUniformBlock perPassBlock(*m_PerPassLayout);
    perPassBlock.WriteRequired("u_ViewProjection", camera.GetViewProjection());
    perPassBlock.WriteRequired("u_LightViewProjection", ctx.Resources.LightViewProjection);
    perPassBlock.WriteRequired("u_CameraPosition", camera.GetPosition());
    perPassBlock.WriteRequired("u_LightDirection", scene.MainDirectionalLight.Direction);
    perPassBlock.WriteRequired("u_LightColor", scene.MainDirectionalLight.Color);
    perPassBlock.WriteRequired("u_LightIntensity", scene.MainDirectionalLight.Intensity);
    perPassBlock.WriteRequired("u_ShadowMapTexelSize", shadowMapTexelSize);

    m_PerPassBuffer->SetData(perPassBlock.Data(), perPassBlock.Size());
    m_Shader->BindUniformBuffer(kForwardPerPassBinding, m_PerPassBuffer);

    for (const auto &item : scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
        {
            LOG_WARN_CAT(LogCategory::Renderer, "ForwardPass: skipping RenderItem with null Mesh or Material");
            continue;
        }

        glm::mat4 model = item.Transform.GetMatrix();
        const glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));

        PackedUniformBlock materialBlock(*m_PerMaterialLayout);
        item.Material->WriteMaterialUniformBlock(materialBlock);
        m_PerMaterialBuffer->SetData(materialBlock.Data(), materialBlock.Size());
        m_Shader->BindUniformBuffer(kForwardPerMaterialBinding, m_PerMaterialBuffer);
        item.Material->BindMaterialTextures(m_Shader);

        PackedUniformBlock drawBlock(*m_PerDrawLayout);
        drawBlock.WriteRequired("u_Model", model);
        drawBlock.WriteRequired("u_NormalMatrix", normalMatrix);
        m_PerDrawBuffer->SetData(drawBlock.Data(), drawBlock.Size());
        m_Shader->BindUniformBuffer(kForwardPerDrawBinding, m_PerDrawBuffer);

        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    RenderCommand::EndRenderPass();
}
