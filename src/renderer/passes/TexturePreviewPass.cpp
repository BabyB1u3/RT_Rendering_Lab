#include "TexturePreviewPass.h"

#include <cassert>

#include "graphics/GraphicsDevice.h"
#include "graphics/Mesh.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/IShader.h"
#include "graphics/interface/ITexture2D.h"
#include "renderer/RenderContext.h"

TexturePreviewPass::TexturePreviewPass()
{
    m_FullscreenQuad = MeshFactory::CreateFullscreenQuad();
    m_Shader = GetDevice()->CreateShader("TexturePreview");
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
    assert(m_Shader && "TexturePreviewPass shader is null");
    assert(m_FullscreenQuad && "TexturePreviewPass fullscreen quad is null");
    assert(ctx.Resources.BackBuffer && "TexturePreviewPass: no back buffer target in context");

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

    assert(texture && "TexturePreviewPass: no texture for current output mode");

    // P2: Explicit render pass — clear the back buffer
    RenderPassDescriptor rpDesc;
    rpDesc.ColorLoadAction = LoadAction::Clear;
    rpDesc.ColorStoreAction = StoreAction::Store;
    rpDesc.ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    rpDesc.DepthLoadAction = LoadAction::DontCare;
    rpDesc.DepthStoreAction = StoreAction::DontCare;

    RenderCommand::BeginRenderPass(ctx.Resources.BackBuffer, rpDesc);

    // P3: Pipeline state — fullscreen quad, no depth/cull/blend
    PipelineState pso;
    pso.DepthTestEnabled = false;
    pso.DepthWriteEnabled = false;
    pso.BlendEnabled = false;
    pso.CullFaceEnabled = false;
    RenderCommand::SetPipelineState(pso);

    m_Shader->Bind();

    // P4: Explicit texture binding — source texture at slot 1
    RenderCommand::SetTexture(1, texture);

    // std140 layout: bool maps to a 4-byte int (0 or 1)
    int32_t isDepthInt = isDepth ? 1 : 0;
    m_Shader->SetUniformBlock(0, &isDepthInt, sizeof(isDepthInt));

    RenderCommand::DrawIndexed(m_FullscreenQuad->GetVertexArray());

    RenderCommand::EndRenderPass();
}
