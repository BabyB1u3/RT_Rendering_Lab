#include "ShadowPass.h"

#include <cassert>
#include <memory>

#include <glm/glm.hpp>

#include "core/Logger.h"
#include "graphics/Framebuffer.h"
#include "graphics/Mesh.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTarget.h"
#include "graphics/Shader.h"
#include "renderer/RenderContext.h"
#include "renderer/RenderItem.h"
#include "scene/SceneData.h"

ShadowPass::ShadowPass(uint32_t width, uint32_t height, const std::string& shaderPath)
    : m_Width(width), m_Height(height)
{
    FramebufferSpecification fbSpec;
    fbSpec.Width = width;
    fbSpec.Height = height;
    fbSpec.Attachments = {
        {TextureFormat::Depth}};

    m_Framebuffer = CreateRef<Framebuffer>(fbSpec);

    m_Shader = Shader::CreateFromSingleFile(shaderPath, "ShadowDepth");
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

Ref<Texture2D> ShadowPass::GetDepthTexture() const
{
    assert(m_Framebuffer && "ShadowPass framebuffer is null");
    return m_Framebuffer->GetDepthAttachment();
}

void ShadowPass::Execute(const RenderContext& ctx)
{
    assert(m_Framebuffer && "ShadowPass framebuffer is null");
    assert(m_Shader && "ShadowPass shader is null");

    RenderTarget target = RenderTarget::FromFramebuffer(m_Framebuffer);
    target.Bind();

    RenderCommand::EnableBlend(false);
    RenderCommand::EnableDepthTest(true);
    RenderCommand::EnableCullFace(true);
    RenderCommand::SetCullFace(true); // Cull front faces to reduce shadow acne

    RenderCommand::SetViewport(0, 0, target.GetWidth(), target.GetHeight());
    RenderCommand::Clear(false, true, false);

    m_Shader->Bind();
    m_Shader->SetMat4("u_LightViewProjection", ctx.Resources.LightViewProjection);

    for (const auto &item : ctx.View.Scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
        {
            LOG_WARN("ShadowPass: skipping RenderItem with null Mesh or Material");
            continue;
        }

        glm::mat4 model = item.Transform.GetMatrix();

        m_Shader->SetMat4("u_Model", model);
        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    RenderCommand::SetCullFace(false); // Restore to cull back faces
    target.Unbind();
}