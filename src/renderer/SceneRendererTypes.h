#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct SceneRendererSpecification
{
    // Shadow pass
    uint32_t ShadowMapWidth = 2048;
    uint32_t ShadowMapHeight = 2048;
    std::string ShadowShaderName = "ShadowDepth";

    // Light projection
    float LightDistance = 10.0f;
    float LightOrthoSize = 10.0f;
    float LightNearPlane = 0.1f;
    float LightFarPlane = 30.0f;

    // Forward pass
    std::string ForwardShaderName = "ForwardLit";
    glm::vec4 ClearColor = {0.1f, 0.1f, 0.12f, 1.0f};

    // Texture preview pass
    std::string TexturePreviewShaderName = "TexturePreview";
};

enum class SceneRendererOutput
{
    FinalColor = 0,
    ShadowMap
};
