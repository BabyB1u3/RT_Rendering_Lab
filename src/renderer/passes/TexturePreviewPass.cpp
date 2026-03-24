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
    // This pass draws to whatever framebuffer is currently bound,
    // so it does not own size-dependent resources for now.
    (void)width;
    (void)height;
}

void TexturePreviewPass::Execute(const RenderContext& ctx)
{
    assert(m_Shader && "TexturePreviewPass shader is null");
    assert(m_FullscreenQuad && "TexturePreviewPass fullscreen quad is null");

    Ref<ITexture2D> texture;
    bool isDepth = false;

    switch (ctx.OutputMode)
    {
    case SceneRendererOutput::FinalColor:
        texture = ctx.Resources.SceneTarget->GetColorAttachment();
        isDepth = false;
        break;

    case SceneRendererOutput::ShadowMap:
        texture = ctx.Resources.ShadowTarget->GetDepthAttachment();
        isDepth = true;
        break;
    }

    assert(texture && "TexturePreviewPass: no texture for current output mode");

    RenderCommand::EnableBlend(false);
    RenderCommand::EnableDepthTest(false);
    RenderCommand::EnableCullFace(false);

    m_Shader->Bind();

    // Slang GLSL layout: sampler2D at binding 1, UBO (GlobalParams) at binding 0
    texture->Bind(1);

    // std140 layout: bool maps to a 4-byte int (0 or 1)
    int32_t isDepthInt = isDepth ? 1 : 0;
    m_Shader->SetUniformBlock(0, &isDepthInt, sizeof(isDepthInt));

    RenderCommand::DrawIndexed(m_FullscreenQuad->GetVertexArray());
}