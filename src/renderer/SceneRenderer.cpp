#include "SceneRenderer.h"

#include <cassert>
#include <memory>

#include "core/Logger.h"

#include <glm/gtc/matrix_transform.hpp>

#include "graphics/Framebuffer.h"
#include "graphics/RenderTarget.h"
#include "graphics/Texture.h"
#include "renderer/RenderContext.h"
#include "renderer/passes/ForwardPass.h"
#include "renderer/passes/ShadowPass.h"
#include "renderer/passes/TexturePreviewPass.h"
#include "scene/Camera.h"
#include "scene/Light.h"
#include "scene/SceneData.h"

SceneRenderer::SceneRenderer(uint32_t width, uint32_t height, const SceneRendererSpecification &spec)
    : m_Width(width), m_Height(height), m_Spec(spec)
{
    m_ShadowPass = CreateRef<ShadowPass>(spec.ShadowMapWidth, spec.ShadowMapHeight, spec.ShadowShaderPath);

    m_ForwardPass = CreateRef<ForwardPass>(width, height, true, spec.ForwardShaderPath, spec.ClearColor);

    m_TexturePreviewPass = CreateRef<TexturePreviewPass>(spec.TexturePreviewShaderPath);

    LOG_INFO("SceneRenderer initialized ({}x{})", width, height);
}

void SceneRenderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    LOG_INFO("SceneRenderer resized to {}x{}", width, height);
    m_Width = width;
    m_Height = height;

    if (m_ForwardPass)
        m_ForwardPass->Resize(width, height);

    if (m_TexturePreviewPass)
        m_TexturePreviewPass->Resize(width, height);
}

glm::mat4 SceneRenderer::BuildDirectionalLightViewProjection(const DirectionalLight &light) const
{
    // Very simple first-pass directional light camera.
    // Later you can replace this with a tighter fit around the visible scene.
    glm::vec3 lightDir = glm::normalize(light.Direction);

    // Pull the light "camera" back opposite the light direction
    glm::vec3 lightPosition = -lightDir * m_Spec.LightDistance;
    glm::vec3 lightTarget = glm::vec3(0.0f);

    glm::mat4 lightView = glm::lookAt(
        lightPosition,
        lightTarget,
        glm::vec3(0.0f, 1.0f, 0.0f));

    // Simple orthographic projection suitable for directional lights
    float orthoSize = m_Spec.LightOrthoSize;
    float nearPlane = m_Spec.LightNearPlane;
    float farPlane = m_Spec.LightFarPlane;

    glm::mat4 lightProjection = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        nearPlane, farPlane);

    return lightProjection * lightView;
}

void SceneRenderer::Render(const SceneData &scene, const Camera &camera)
{
    assert(m_ShadowPass && m_ForwardPass && m_TexturePreviewPass && "SceneRenderer passes are not initialized");

    SceneView view{scene, camera, m_Width, m_Height};

    FrameResources resources;
    resources.LightViewProjection = BuildDirectionalLightViewProjection(scene.MainDirectionalLight);
    resources.ShadowMap = m_ShadowPass->GetDepthTexture();
    resources.ShadowTarget = RenderTarget::FromFramebuffer(m_ShadowPass->GetFramebuffer());
    resources.SceneColor = m_ForwardPass->GetFramebuffer()->GetColorAttachment(0);
    resources.SceneTarget = RenderTarget::FromFramebuffer(m_ForwardPass->GetFramebuffer());

    RenderContext ctx{view, m_Spec, std::move(resources), m_OutputMode};

    // 1. Shadow map generation
    m_ShadowPass->Execute(ctx);

    // 2. Forward render to scene color target
    m_ForwardPass->Execute(ctx);

    // 3. Present selected output to backbuffer (pass decides based on OutputMode)
    m_TexturePreviewPass->Execute(ctx);
}