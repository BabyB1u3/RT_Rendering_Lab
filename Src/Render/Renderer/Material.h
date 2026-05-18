#pragma once

/// @file Material.h
/// @brief Minimal material data consumed by the forward renderer.

#include "Core/Util/Math.h"
#include "Render/RHI/RHIResources.h"

namespace Renderer
{
struct Material
{
    Math::Vec4 m_BaseColor = Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    TextureView* m_AlbedoTextureView = nullptr;
    Sampler* m_AlbedoSampler = nullptr;
    ResourceSet* m_ResourceSet = nullptr;
};
} // namespace Renderer
