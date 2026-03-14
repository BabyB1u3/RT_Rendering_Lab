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
    TexturePreviewPass(const std::string& shaderPath);

    void Resize(unsigned int width, unsigned int height) override;
    void Execute(const RenderContext& ctx) override;

private:
    Ref<Shader> m_Shader;
    Ref<Mesh> m_FullscreenQuad;
};