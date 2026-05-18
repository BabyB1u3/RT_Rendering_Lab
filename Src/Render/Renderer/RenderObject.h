#pragma once

/// @file RenderObject.h
/// @brief Minimal render-object data connecting a mesh, material, and transform.

#include "Core/Util/Math.h"
#include "Render/RHI/RHIResources.h"
#include "Render/Renderer/Material.h"
#include "Render/Renderer/Mesh.h"

namespace Renderer
{
struct RenderObject
{
    const Mesh* m_Mesh = nullptr;
    Material* m_Material = nullptr;
    ResourceSet* m_ObjectSet = nullptr;
    Math::Mat4 m_Model = Math::Mat4::Identity();
};
} // namespace Renderer
