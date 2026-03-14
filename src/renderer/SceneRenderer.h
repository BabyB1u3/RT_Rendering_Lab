#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "renderer/SceneRendererTypes.h"

class Camera;
class Framebuffer;
class Texture2D;
class ShadowPass;
class ForwardPass;
class TexturePreviewPass;
struct SceneData;
struct DirectionalLight;

class SceneRenderer
{
public:
    SceneRenderer(uint32_t width, uint32_t height, const SceneRendererSpecification &spec = {});

    void Resize(uint32_t width, uint32_t height);

    void SetOutputMode(SceneRendererOutput mode) { m_OutputMode = mode; }
    SceneRendererOutput GetOutputMode() const { return m_OutputMode; }

    void Render(const SceneData &scene, const Camera &camera);

    Ref<ShadowPass> GetShadowPass() const { return m_ShadowPass; }
    Ref<ForwardPass> GetForwardPass() const { return m_ForwardPass; }

private:
    glm::mat4 BuildDirectionalLightViewProjection(const DirectionalLight &light) const;

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    SceneRendererOutput m_OutputMode = SceneRendererOutput::FinalColor;

    SceneRendererSpecification m_Spec;

    Ref<ShadowPass> m_ShadowPass;
    Ref<ForwardPass> m_ForwardPass;
    Ref<TexturePreviewPass> m_TexturePreviewPass;
};