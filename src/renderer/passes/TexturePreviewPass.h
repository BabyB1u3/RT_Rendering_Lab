#pragma once

#include <memory>
#include <string>

#include "core/Base.h"
#include "RenderPass.h"

class Shader;
class Mesh;
class Texture2D;

class TexturePreviewPass : public RenderPass
{
public:
    TexturePreviewPass(const std::string& shaderPath = "assets/shaders/TexturePreview.glsl");

    void Resize(unsigned int width, unsigned int height) override;

    // Draw the texture to the currently bound framebuffer / backbuffer
    void Execute(const Ref<Texture2D> &texture, bool isDepthTexture = false);

private:
    Ref<Shader> m_Shader;
    Ref<Mesh> m_FullscreenQuad;
};