#include "TexturePreviewPass.h"

#include "core/diagnostics/Assert.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Mesh.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"
#include "graphics/interfaces/IUniformBuffer.h"
#include "renderer/RenderContext.h"

namespace
{
    constexpr ShaderBindingPoint kTexturePreviewParamsBinding{0, 0};
    constexpr ShaderBindingPoint kTexturePreviewTextureBinding{0, 1};
}

TexturePreviewPass::TexturePreviewPass()
{
    m_FullscreenQuad = MeshFactory::CreateFullscreenQuad();
    RTRLAB_ASSERT_MSG(m_FullscreenQuad, "TexturePreviewPass failed to create fullscreen quad");
    m_Shader = GetDevice()->CreateShader("TexturePreview");
    RTRLAB_ASSERT_MSG(m_Shader, "TexturePreviewPass failed to create TexturePreview shader");
    m_UniformBlockLayout = m_Shader->GetUniformBlockLayout(kTexturePreviewParamsBinding);
    RTRLAB_ASSERT_MSG(m_UniformBlockLayout,
                      "TexturePreviewPass: shader must provide reflected layout for logical binding {0, 0}.");
    m_UniformBuffer = GetDevice()->CreateUniformBuffer(m_UniformBlockLayout->GetSize());
    RTRLAB_ASSERT_MSG(m_UniformBuffer, "TexturePreviewPass failed to create uniform buffer for logical binding {0, 0}");
}

void TexturePreviewPass::Resize(unsigned int width, unsigned int height)
{
    // This pass draws to the back buffer (provided via RenderContext),
    // so it does not own size-dependent resources for now.
    (void)width;
    (void)height;
}

void TexturePreviewPass::Execute(const RenderContext &ctx)
{
    RTRLAB_ASSERT_MSG(m_Shader, "TexturePreviewPass shader is null");
    RTRLAB_ASSERT_MSG(m_FullscreenQuad, "TexturePreviewPass fullscreen quad is null");
    RTRLAB_ASSERT_MSG(ctx.Resources.BackBuffer, "TexturePreviewPass: no back buffer target in context");

    Ref<ITexture2D> texture;
    bool isDepth = false;

    switch (ctx.OutputMode)
    {
    case SceneRendererOutput::FinalColor:
        texture = ctx.Resources.SceneColor;
        isDepth = false;
        break;

    case SceneRendererOutput::ShadowMap:
        texture = ctx.Resources.ShadowMap;
        isDepth = true;
        break;
    }

    RTRLAB_ASSERT_MSG(texture, "TexturePreviewPass: no texture for current output mode");

    // P2: Explicit render pass - clear the back buffer
    RenderPassDescriptor rpDesc;
    rpDesc.ColorLoadAction = LoadAction::Clear;
    rpDesc.ColorStoreAction = StoreAction::Store;
    rpDesc.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    rpDesc.DepthLoadAction = LoadAction::DontCare;
    rpDesc.DepthStoreAction = StoreAction::DontCare;

    RenderCommand::BeginRenderPass(ctx.Resources.BackBuffer, rpDesc);

    // P3: Pipeline state - fullscreen quad, no depth/cull/blend
    PipelineState pso;
    pso.DepthTestEnabled = false;
    pso.DepthWriteEnabled = false;
    pso.BlendEnabled = false;
    pso.CullFaceEnabled = false;
    RenderCommand::SetPipelineState(pso);

    m_Shader->Bind();
    RTRLAB_ASSERT_MSG(m_UniformBlockLayout, "TexturePreviewPass uniform block layout is null");
    RTRLAB_ASSERT_MSG(m_UniformBuffer, "TexturePreviewPass uniform buffer is null");

    // Pilot set-aware migration: bind preview resources through logical {set, binding}.
    m_Shader->BindTexture(kTexturePreviewTextureBinding, texture);

    PackedUniformBlock block(*m_UniformBlockLayout);
    block.WriteRequired("u_IsDepthTexture", isDepth);
    m_UniformBuffer->SetData(block.Data(), block.Size());
    m_Shader->BindUniformBuffer(kTexturePreviewParamsBinding, m_UniformBuffer);

    RenderCommand::DrawIndexed(m_FullscreenQuad->GetVertexArray());

    RenderCommand::EndRenderPass();
}
