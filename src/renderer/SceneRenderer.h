#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/Base.h"

class Framebuffer;
class Texture2D;
class ShadowPass;
class ForwardPass;
class TexturePreviewPass;
struct SceneData;

struct SceneRendererSpecification
{
    // Shadow pass
    uint32_t ShadowMapWidth  = 2048;
    uint32_t ShadowMapHeight = 2048;
    std::string ShadowShaderPath = "assets/shaders/ShadowDepth.glsl";

    // Light projection
    float LightDistance  = 10.0f;
    float LightOrthoSize = 10.0f;
    float LightNearPlane = 0.1f;
    float LightFarPlane  = 30.0f;

    // Forward pass
    std::string ForwardShaderPath = "assets/shaders/ForwardLit.glsl";
    glm::vec4 ClearColor = { 0.1f, 0.1f, 0.12f, 1.0f };

    // Texture preview pass
    std::string TexturePreviewShaderPath = "assets/shaders/TexturePreview.glsl";
};

enum class SceneRendererOutput
{
    FinalColor = 0,
    ShadowMap
};

class SceneRenderer
{
public:
    SceneRenderer(uint32_t width, uint32_t height, const SceneRendererSpecification& spec = {});

    void Resize(uint32_t width, uint32_t height);

    void SetOutputMode(SceneRendererOutput mode) { m_OutputMode = mode; }
    SceneRendererOutput GetOutputMode() const { return m_OutputMode; }

    void Render(const SceneData &scene);

    Ref<ShadowPass> GetShadowPass() const { return m_ShadowPass; }
    Ref<ForwardPass> GetForwardPass() const { return m_ForwardPass; }

private:
    glm::mat4 BuildDirectionalLightViewProjection(const SceneData &scene) const;

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    SceneRendererOutput m_OutputMode = SceneRendererOutput::FinalColor;

    SceneRendererSpecification m_Spec;

    Ref<ShadowPass> m_ShadowPass;
    Ref<ForwardPass> m_ForwardPass;
    Ref<TexturePreviewPass> m_TexturePreviewPass;
};