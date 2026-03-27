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

    // Slang UBO layout (binding 0, std140, row_major):
    //   mat4 u_ViewProjection       offset   0
    //   mat4 u_Model                offset  64
    //   mat4 u_NormalMatrix         offset 128
    //   mat4 u_LightViewProjection  offset 192
    //   vec3 u_CameraPosition       offset 256  (+ 4 pad)
    //   vec3 u_LightDirection       offset 272  (+ 4 pad)
    //   vec3 u_LightColor           offset 288
    //   float u_LightIntensity      offset 300
    //   vec3 u_Albedo               offset 304
    //   float u_SpecularPower       offset 316
    //   float u_AmbientStrength     offset 320
    //   bool(int) u_UseAlbedoMap    offset 324
    //   vec2 u_ShadowMapTexelSize   offset 328
    struct alignas(16) ForwardParams
    {
        glm::mat4 ViewProjection;      // 0
        glm::mat4 Model;               // 64
        glm::mat4 NormalMatrix;        // 128
        glm::mat4 LightViewProjection; // 192
        glm::vec3 CameraPosition;      // 256
        float _pad0{};                 // 268
        glm::vec3 LightDirection;      // 272
        float _pad1{};                 // 284
        glm::vec3 LightColor;          // 288
        float LightIntensity;          // 300
        glm::vec3 Albedo;              // 304
        float SpecularPower;           // 316
        float AmbientStrength;         // 320
        int32_t UseAlbedoMap;          // 324  (std140 bool = 4 bytes)
        glm::vec2 ShadowMapTexelSize;  // 328
    };

    ForwardParams params{};
    params.ViewProjection = camera.GetViewProjection();
    params.LightViewProjection = ctx.Resources.LightViewProjection;
    params.CameraPosition = camera.GetPosition();
    params.LightDirection = scene.MainDirectionalLight.Direction;
    params.LightColor = scene.MainDirectionalLight.Color;
    params.LightIntensity = scene.MainDirectionalLight.Intensity;

    // P4: Explicit texture binding - shadow map at slot 1
    const auto &shadow = ctx.Resources.ShadowMap ? ctx.Resources.ShadowMap : m_FallbackShadowMap;
    RenderCommand::SetTexture(1, shadow);
    params.ShadowMapTexelSize = {
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
        params.Model = model;
        params.NormalMatrix = glm::transpose(glm::inverse(model));

        // Read material properties
        params.Albedo = item.Material->GetVec3("u_Albedo", glm::vec3(1.0f));
        params.SpecularPower = item.Material->GetFloat("u_SpecularPower", 32.0f);
        params.AmbientStrength = item.Material->GetFloat("u_AmbientStrength", 0.1f);

        auto albedoTex = item.Material->GetTexture(TextureSlot::Albedo);
        if (albedoTex)
        {
            // P4: Explicit texture binding - albedo at slot 2
            RenderCommand::SetTexture(2, albedoTex);
            params.UseAlbedoMap = 1;
        }
        else
        {
            params.UseAlbedoMap = 0;
        }

        m_Shader->SetUniformBlock(0, &params, sizeof(params));
        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    RenderCommand::EndRenderPass();
}
