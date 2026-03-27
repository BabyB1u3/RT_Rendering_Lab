#include "ShadowPass.h"

#include <memory>

#include <glm/glm.hpp>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/Framebuffer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Mesh.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "renderer/RenderContext.h"
#include "renderer/RenderItem.h"
#include "scene/SceneData.h"

ShadowPass::ShadowPass(uint32_t width, uint32_t height)
    : m_Width(width), m_Height(height)
{
    FramebufferSpecification fbSpec;
    fbSpec.Width = width;
    fbSpec.Height = height;
    fbSpec.Attachments = {
        {TextureFormat::Depth}};

    m_Framebuffer = GetDevice()->CreateFramebuffer(fbSpec);
    m_RenderTarget = GetDevice()->CreateRenderTargetFromFramebuffer(m_Framebuffer);

    m_Shader = GetDevice()->CreateShader("ShadowDepth");
    RTRLAB_ASSERT_MSG(m_Shader, "ShadowPass failed to create ShadowDepth shader");
}

void ShadowPass::Resize(unsigned int width, unsigned int height)
{
    if (width == 0 || height == 0)
        return;

    m_Width = width;
    m_Height = height;

    if (m_Framebuffer)
        m_Framebuffer->Resize(width, height);
}

Ref<ITexture2D> ShadowPass::GetDepthTexture() const
{
    RTRLAB_ASSERT_MSG(m_Framebuffer, "ShadowPass framebuffer is null");
    return m_Framebuffer->GetDepthAttachment();
}

void ShadowPass::Execute(const RenderContext &ctx)
{
    RTRLAB_ASSERT_MSG(m_Framebuffer, "ShadowPass framebuffer is null");
    RTRLAB_ASSERT_MSG(m_Shader, "ShadowPass shader is null");

    // P2: Explicit render pass descriptor - clear depth, no color attachment
    RenderPassDescriptor rpDesc;
    rpDesc.ColorLoadAction = LoadAction::DontCare;
    rpDesc.ColorStoreAction = StoreAction::DontCare;
    rpDesc.DepthLoadAction = LoadAction::Clear;
    rpDesc.DepthStoreAction = StoreAction::Store;
    rpDesc.ClearDepth = 1.0f;

    RenderCommand::BeginRenderPass(m_RenderTarget, rpDesc);

    // P3: Pipeline state - depth only, cull front faces to reduce shadow acne
    PipelineState pso;
    pso.DepthTestEnabled = true;
    pso.DepthWriteEnabled = true;
    pso.BlendEnabled = false;
    pso.CullFaceEnabled = true;
    pso.CullFront = true;
    RenderCommand::SetPipelineState(pso);

    RenderCommand::SetViewport(0, 0, m_RenderTarget->GetWidth(), m_RenderTarget->GetHeight());

    m_Shader->Bind();
    const ShaderUniformBlockLayout *blockLayout = m_Shader->GetUniformBlockLayout(0);
    RTRLAB_ASSERT_MSG(blockLayout,
                      "ShadowPass: shader must provide reflected layout for uniform block binding 0.");

    for (const auto &item : ctx.View.Scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
        {
            LOG_WARN_CAT(LogCategory::Renderer, "ShadowPass: skipping RenderItem with null Mesh or Material");
            continue;
        }

        const glm::mat4 model = item.Transform.GetMatrix();

        PackedUniformBlock block(*blockLayout);
        auto requireWrite = [&](const char *fieldName, const auto &value)
        {
            RTRLAB_ASSERTF(block.Write(fieldName, value), "{}", block.GetLastError());
        };

        requireWrite("u_LightViewProjection", ctx.Resources.LightViewProjection);
        requireWrite("u_Model", model);
        m_Shader->SetUniformBlock(0, block.Data(), block.Size());

        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    RenderCommand::EndRenderPass();
}
