#pragma once

/// @file Light.h
/// @brief Light source definitions used by the scene and render passes.
///
/// Currently only DirectionalLight is supported (infinite distance, parallel rays).
/// The direction vector points FROM the light TOWARD the surface (i.e., the incident
/// direction). It is normalized before use by the ShadowPass and ForwardPass.
///
/// The padding field ensures 16-byte alignment for potential future UBO upload.

#include "Core/Util/Math.h"

struct DirectionalLight
{
    Math::Vec3 m_Direction = Math::Vec3(-0.5f, -1.0f, -0.3f).normalized();
    float m_Intensity = 1.0f;

    Math::Vec3 m_Color{1.0f, 1.0f, 1.0f};
    float m_Padding = 0.0f; // Align to 16 bytes for potential future UBO layout
};
