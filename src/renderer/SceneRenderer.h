#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "core/Base.h"

class Framebuffer;
class Texture2D;
class ShadowPass;
class ForwardPass;
class TexturePreviewPass;
struct SceneData;

enum class SceneRendererOutput
{
    FinalColor = 0,
    ShadowMap
};

class SceneRenderer
{
public:
    SceneRenderer(uint32_t width, uint32_t height);

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

    Ref<ShadowPass> m_ShadowPass;
    Ref<ForwardPass> m_ForwardPass;
    Ref<TexturePreviewPass> m_TexturePreviewPass;
};