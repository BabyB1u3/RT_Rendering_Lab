#include "TexturePreviewPass.h"

#include <cassert>

#include "graphics/Mesh.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "renderer/RenderContext.h"

TexturePreviewPass::TexturePreviewPass(const std::string& shaderPath)
{
    m_FullscreenQuad = MeshFactory::CreateFullscreenQuad();
    m_Shader = Shader::CreateFromSingleFile(shaderPath, "TexturePreview");
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

    Ref<Texture2D> texture;
    bool isDepth = false;

    switch (ctx.OutputMode)
    {
    case SceneRendererOutput::FinalColor:
        texture = ctx.Resources.SceneTarget.GetColorAttachment();
        isDepth = false;
        break;

    case SceneRendererOutput::ShadowMap:
        texture = ctx.Resources.ShadowTarget.GetDepthAttachment();
        isDepth = true;
        break;
    }

    assert(texture && "TexturePreviewPass: no texture for current output mode");

    RenderCommand::EnableBlend(false);
    RenderCommand::EnableDepthTest(false);
    RenderCommand::EnableCullFace(false);

    m_Shader->Bind();

    texture->Bind(0);
    m_Shader->SetInt("u_Texture", 0);
    m_Shader->SetBool("u_IsDepthTexture", isDepth);

    RenderCommand::DrawIndexed(m_FullscreenQuad->GetVertexArray());
}