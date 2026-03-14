#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "RenderPass.h"

class Framebuffer;
class Shader;
class Texture2D;
struct SceneData;

class ForwardPass : public RenderPass
{
public:
    ForwardPass(uint32_t width, uint32_t height, bool renderToTarget = true,
                const std::string& shaderPath = "assets/shaders/ForwardLit.glsl",
                const glm::vec4& clearColor = { 0.1f, 0.1f, 0.12f, 1.0f });

    void Resize(unsigned int width, unsigned int height) override;

    void Execute(
        const SceneData &scene,
        const Ref<Texture2D> &shadowMap,
        const glm::mat4 &lightViewProjection);

    Ref<Framebuffer> GetFramebuffer() const { return m_Framebuffer; }

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    bool m_RenderToTarget = true;

    Ref<Framebuffer> m_Framebuffer;
    glm::vec4 m_ClearColor = { 0.1f, 0.1f, 0.12f, 1.0f };

    Ref<Shader> m_Shader;
    Ref<Texture2D> m_FallbackShadowMap;
};