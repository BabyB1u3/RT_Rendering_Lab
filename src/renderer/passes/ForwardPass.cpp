#include "renderer/passes/ForwardPass.h"

#include <cassert>
#include <memory>

#include <glm/glm.hpp>

#include "core/Logger.h"
#include "graphics/Framebuffer.h"
#include "graphics/Material.h"
#include "graphics/Mesh.h"
#include "graphics/RenderCommand.h"
#include "graphics/RenderTarget.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "renderer/RenderItem.h"
#include "scene/Camera.h"
#include "scene/SceneData.h"

ForwardPass::ForwardPass(uint32_t width, uint32_t height, bool renderToTarget,
                         const std::string& shaderPath, const glm::vec4& clearColor)
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

        m_Framebuffer = CreateRef<Framebuffer>(fbSpec);
    }

    m_Shader = Shader::CreateFromSingleFile(shaderPath, "ForwardLit");

    // 1x1 white fallback texture for when no shadow map is provided.
    // Sampling r = 1.0 means currentDepth - bias > 1.0 is always false -> no shadow.
    TextureSpecification fallbackSpec;
    fallbackSpec.Width = 1;
    fallbackSpec.Height = 1;
    fallbackSpec.Format = TextureFormat::RGBA8;
    m_FallbackShadowMap = Texture2D::Create(fallbackSpec);
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

void ForwardPass::Execute(
    const SceneData &scene,
    const Ref<Texture2D> &shadowMap,
    const glm::mat4 &lightViewProjection)
{
    assert(scene.ActiveCamera && "ForwardPass requires an active camera");
    assert(m_Shader && "ForwardPass shader is null");

    RenderTarget target = m_RenderToTarget
        ? RenderTarget::FromFramebuffer(m_Framebuffer)
        : RenderTarget::BackBuffer(m_Width, m_Height);
    target.Bind();

    RenderCommand::EnableBlend(false);
    RenderCommand::EnableDepthTest(true);
    RenderCommand::EnableCullFace(true);

    RenderCommand::SetViewport(0, 0, target.GetWidth(), target.GetHeight());
    RenderCommand::SetClearColor(m_ClearColor);
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
    shadow->Bind(static_cast<uint32_t>(TextureSlot::ShadowMap));
    m_Shader->SetInt("u_ShadowMap", static_cast<int>(TextureSlot::ShadowMap));

    for (const auto &item : scene.RenderItems)
    {
        if (!item.Mesh || !item.Material)
        {
            LOG_WARN("ForwardPass: skipping RenderItem with null Mesh or Material");
            continue;
        }

        glm::mat4 model = item.Transform.GetMatrix();

        m_Shader->SetMat4("u_Model", model);
        m_Shader->SetMat3("u_NormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));

        item.Material->UploadToShader(m_Shader);

        RenderCommand::DrawIndexed(item.Mesh->GetVertexArray());
    }

    target.Unbind();
}