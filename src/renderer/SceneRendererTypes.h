#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct SceneRendererSpecification
{
    // Shadow pass
    uint32_t ShadowMapWidth = 2048;
    uint32_t ShadowMapHeight = 2048;
    std::string ShadowShaderPath = "assets/shaders/ShadowDepth.glsl";

    // Light projection
    float LightDistance = 10.0f;
    float LightOrthoSize = 10.0f;
    float LightNearPlane = 0.1f;
    float LightFarPlane = 30.0f;

    // Forward pass
    std::string ForwardShaderPath = "assets/shaders/ForwardLit.glsl";
    glm::vec4 ClearColor = {0.1f, 0.1f, 0.12f, 1.0f};

    // Texture preview pass
    std::string TexturePreviewShaderPath = "assets/shaders/TexturePreview.glsl";
};

enum class SceneRendererOutput
{
    FinalColor = 0,
    ShadowMap
};
