#pragma once

/// @file SceneRendererTypes.h
/// @brief Configuration and output-mode types for SceneRenderer.
///
/// SceneRendererSpecification holds all tuneable parameters for the multi-pass
/// pipeline - shadow resolution, light projection bounds, shader names, clear color.
/// Demos expose a mutable reference to this spec via SceneRenderer::GetSpecification()
/// so that ImGui panels can adjust parameters at runtime.

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct SceneRendererSpecification
{
    // --- Shadow pass ---
    uint32_t ShadowMapWidth = 2048;
    uint32_t ShadowMapHeight = 2048;
    std::string ShadowShaderName = "ShadowDepth";

    // --- Light projection (orthographic, for directional light) ---
    float LightDistance = 10.0f;  // How far back the virtual light camera sits
    float LightOrthoSize = 10.0f; // Half-extent of the ortho frustum
    float LightNearPlane = 0.1f;
    float LightFarPlane = 30.0f;

    // --- Forward pass ---
    std::string ForwardShaderName = "ForwardLit";
    glm::vec4 ClearColor = {0.1f, 0.1f, 0.12f, 1.0f};

    // --- Texture preview pass ---
    std::string TexturePreviewShaderName = "TexturePreview";
};

/// Selects which intermediate texture is presented to the back buffer.
enum class SceneRendererOutput
{
    FinalColor = 0, // Scene color from ForwardPass
    ShadowMap       // Shadow depth buffer (grayscale debug view)
};
