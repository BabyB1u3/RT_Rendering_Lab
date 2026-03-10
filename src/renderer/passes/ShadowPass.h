#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "RenderPass.h"

class Framebuffer;
class Shader;
class Texture2D;
struct SceneData;

class ShadowPass : public RenderPass
{
public:
    ShadowPass(uint32_t width, uint32_t height);

    void Resize(unsigned int width, unsigned int height) override;

    void Execute(const SceneData &scene, const glm::mat4 &lightViewProjection);

    Ref<Framebuffer> GetFramebuffer() const { return m_Framebuffer; }
    Ref<Texture2D> GetDepthTexture() const;

    const glm::mat4 &GetLightViewProjection() const { return m_LightViewProjection; }

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    glm::mat4 m_LightViewProjection{1.0f};

    Ref<Framebuffer> m_Framebuffer;
    Ref<Shader> m_Shader;
};