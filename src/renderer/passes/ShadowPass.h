#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "core/Base.h"
#include "RenderPass.h"

class Framebuffer;
class Shader;
class Texture2D;

class ShadowPass : public RenderPass
{
public:
    ShadowPass(uint32_t width, uint32_t height,
               const std::string& shaderPath);

    void Resize(unsigned int width, unsigned int height) override;
    void Execute(const RenderContext& ctx) override;

    Ref<Framebuffer> GetFramebuffer() const { return m_Framebuffer; }
    Ref<Texture2D> GetDepthTexture() const;

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    Ref<Framebuffer> m_Framebuffer;
    Ref<Shader> m_Shader;
};