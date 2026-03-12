#include "ShadowPass.h"

#include <cassert>
#include <memory>

#include <glm/glm.hpp>

#include "graphics/Framebuffer.h"
#include "graphics/Mesh.h"
#include "graphics/RenderCommand.h"
#include "graphics/Shader.h"
#include "scene/SceneData.h"
#include "renderer/RenderItem.h"

ShadowPass::ShadowPass(uint32_t width, uint32_t height)
    : m_Width(width), m_Height(height)
{
    FramebufferSpecification fbSpec;
    fbSpec.Width = width;
    fbSpec.Height = height;
    fbSpec.Attachments = {
        {TextureFormat::Depth}};

    m_Framebuffer = CreateRef<Framebuffer>(fbSpec);

    m_Shader = Shader::CreateFromSingleFile("assets/shaders/ShadowDepth.glsl", "ShadowDepth");
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

void ShadowPass::Execute(const SceneData &scene, const glm::mat4 &lightViewProjection)
{
    assert(m_Framebuffer && "ShadowPass framebuffer is null");
    assert(m_Shader && "ShadowPass shader is null");

    m_LightViewProjection = lightViewProjection;

    m_Framebuffer->Bind();

    RenderCommand::EnableBlend(false);
    RenderCommand::EnableDepthTest(true);
    RenderCommand::EnableCullFace(true);
    RenderCommand::SetCullFace(true); // Cull front faces to reduce shadow acne

    RenderCommand::SetViewport(0, 0, m_Width, m_Height);
    RenderCommand::Clear(false, true, false);

    m_Shader->Bind();
    m_Shader->SetMat4("u_LightViewProjection", m_LightViewProjection);

    for (const auto &item : scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
            continue;

        glm::mat4 model = item.Transform.GetMatrix();

        m_Shader->SetMat4("u_Model", model);
        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    RenderCommand::SetCullFace(false); // Restore to cull back faces
    m_Framebuffer->Unbind();
}