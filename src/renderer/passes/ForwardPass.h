#pragma once

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "renderer/passes/RenderPass.h"

class Framebuffer;
class Shader;
class Texture2D;
struct SceneData;

class ForwardPass : public RenderPass
{
public:
    ForwardPass(uint32_t width, uint32_t height, bool renderToTarget = true);

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
    Ref<Shader> m_Shader;
};