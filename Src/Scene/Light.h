#pragma once

/// @file Light.h
/// @brief Light source definitions used by the scene and render passes.
///
/// Currently only DirectionalLight is supported (infinite distance, parallel rays).
/// The Direction vector points FROM the light TOWARD the surface (i.e., the incident
/// direction). It is normalized before use by the ShadowPass and ForwardPass.
///
/// The Padding field ensures 16-byte alignment for potential future UBO upload.

#include <glm/glm.hpp>

struct DirectionalLight
{
    glm::vec3 Direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    float Intensity = 1.0f;

    glm::vec3 Color{1.0f, 1.0f, 1.0f};
    float Padding = 0.0f;  // Align to 16 bytes for potential UBO layout
};