#include "renderer/passes/ForwardPass.h"

#include <cassert>
#include <memory>

#include <glm/glm.hpp>

#include "graphics/Framebuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/RenderCommand.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "renderer/RenderItem.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

ForwardPass::ForwardPass(uint32_t width, uint32_t height, bool renderToTarget)
    : m_Width(width), m_Height(height), m_RenderToTarget(renderToTarget)
{
    if (m_RenderToTarget)
    {
        FramebufferSpecification fbSpec;
        fbSpec.Width = width;
        fbSpec.Height = height;
        fbSpec.Attachments = {
            {TextureFormat::RGBA8},
            {TextureFormat::Depth24Stencil8}};

        m_Framebuffer = CreateRef<Framebuffer>(fbSpec);
    }

    m_Shader = Shader::CreateFromSingleFile("assets/shaders/ForwardLit.glsl", "ForwardLit");

    // 1x1 white fallback texture for when no shadow map is provided.
    // Sampling r = 1.0 means currentDepth - bias > 1.0 is always false -> no shadow.
    TextureSpecification fallbackSpec;
    fallbackSpec.Width = 1;
    fallbackSpec.Height = 1;
    fallbackSpec.Format = TextureFormat::RGBA8;
    m_FallbackShadowMap = Texture2D::Create(fallbackSpec);
    const uint32_t white = 0xFFFFFFFFu;
    m_FallbackShadowMap->SetData(&white, sizeof(uint32_t));
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

void ForwardPass::Execute(
    const SceneData &scene,
    const Ref<Texture2D> &shadowMap,
    const glm::mat4 &lightViewProjection)
{
    assert(scene.ActiveCamera && "ForwardPass requires an active camera");
    assert(m_Shader && "ForwardPass shader is null");

    if (m_RenderToTarget)
        m_Framebuffer->Bind();

    RenderCommand::EnableBlend(false);
    RenderCommand::EnableDepthTest(true);
    RenderCommand::EnableCullFace(true);

    RenderCommand::SetViewport(0, 0, m_Width, m_Height);
    RenderCommand::SetClearColor({0.1f, 0.1f, 0.12f, 1.0f});
    RenderCommand::Clear(true, true, false);

    Camera *camera = scene.ActiveCamera;

    m_Shader->Bind();
    m_Shader->SetMat4("u_ViewProjection", camera->GetViewProjection());
    m_Shader->SetMat4("u_LightViewProjection", lightViewProjection);
    m_Shader->SetFloat3("u_CameraPosition", camera->GetPosition());
    m_Shader->SetFloat3("u_LightDirection", scene.MainDirectionalLight.Direction);
    m_Shader->SetFloat3("u_LightColor", scene.MainDirectionalLight.Color);
    m_Shader->SetFloat("u_LightIntensity", scene.MainDirectionalLight.Intensity);

    const auto &shadow = shadowMap ? shadowMap : m_FallbackShadowMap;
    shadow->Bind(0);
    m_Shader->SetInt("u_ShadowMap", 0);

    for (const auto &item : scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
            continue;

        glm::mat4 model = item.Transform.GetMatrix();

        m_Shader->SetMat4("u_Model", model);

        // Bind material resources after the pass shader is bound.
        // Current minimal Material only binds textures + its own shader,
        // so for now we bind textures manually to avoid switching shaders.
        auto albedo = item.Material->GetTexture(1);
        if (albedo)
        {
            albedo->Bind(1);
            m_Shader->SetInt("u_AlbedoMap", 1);
            m_Shader->SetBool("u_UseAlbedoMap", true);
        }
        else
        {
            m_Shader->SetBool("u_UseAlbedoMap", false);
        }

        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    if (m_RenderToTarget)
        m_Framebuffer->Unbind();
}