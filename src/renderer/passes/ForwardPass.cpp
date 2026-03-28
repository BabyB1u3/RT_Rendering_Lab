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
#include "renderer/RenderContext.h"
#include "renderer/RenderItem.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

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
    const ShaderUniformBlockLayout *blockLayout = m_Shader->GetUniformBlockLayout(0);
    RTRLAB_ASSERT_MSG(blockLayout,
                      "ForwardPass: shader must provide reflected layout for uniform block binding 0.");

    // P5a: Shader-scoped texture binding - shadow map at slot 1
    const auto &shadow = ctx.Resources.ShadowMap ? ctx.Resources.ShadowMap : m_FallbackShadowMap;
    m_Shader->BindTexture(1, shadow);
    const glm::vec2 shadowMapTexelSize = {
        1.0f / static_cast<float>(shadow->GetWidth()),
        1.0f / static_cast<float>(shadow->GetHeight())};

    for (const auto &item : scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
        {
            LOG_WARN_CAT(LogCategory::Renderer, "ForwardPass: skipping RenderItem with null Mesh or Material");
            continue;
        }

        glm::mat4 model = item.Transform.GetMatrix();
        const glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));

        // Read material properties
        const glm::vec3 albedo = item.Material->GetVec3("u_Albedo", glm::vec3(1.0f));
        const float specularPower = item.Material->GetFloat("u_SpecularPower", 32.0f);
        const float ambientStrength = item.Material->GetFloat("u_AmbientStrength", 0.1f);

        auto albedoTex = item.Material->GetTexture(TextureSlot::Albedo);
        const bool useAlbedoMap = (albedoTex != nullptr);
        if (albedoTex)
        {
            // P5a: Shader-scoped texture binding - albedo at slot 2
            m_Shader->BindTexture(2, albedoTex);
        }

        PackedUniformBlock block(*blockLayout);
        block.WriteRequired("u_ViewProjection", camera.GetViewProjection());
        block.WriteRequired("u_Model", model);
        block.WriteRequired("u_NormalMatrix", normalMatrix);
        block.WriteRequired("u_LightViewProjection", ctx.Resources.LightViewProjection);
        block.WriteRequired("u_CameraPosition", camera.GetPosition());
        block.WriteRequired("u_LightDirection", scene.MainDirectionalLight.Direction);
        block.WriteRequired("u_LightColor", scene.MainDirectionalLight.Color);
        block.WriteRequired("u_LightIntensity", scene.MainDirectionalLight.Intensity);
        block.WriteRequired("u_Albedo", albedo);
        block.WriteRequired("u_SpecularPower", specularPower);
        block.WriteRequired("u_AmbientStrength", ambientStrength);
        block.WriteRequired("u_UseAlbedoMap", useAlbedoMap);
        block.WriteRequired("u_ShadowMapTexelSize", shadowMapTexelSize);

        m_Shader->SetUniformBlock(0, block.Data(), block.Size());

        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    RenderCommand::EndRenderPass();
}
