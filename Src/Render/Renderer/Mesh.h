#pragma once

/// @file Mesh.h
/// @brief Minimal mesh binding data consumed by the forward renderer.

#include <cstdint>

#include "Render/RHI/RHIPipeline.h"

namespace Renderer
{
struct Mesh
{
    Buffer* m_VertexBuffer = nullptr;
    Buffer* m_IndexBuffer = nullptr;
    IndexType m_IndexType = IndexType::UInt16;
    uint32_t m_IndexCount = 0;
    uint32_t m_FirstIndex = 0;
    int32_t m_VertexOffset = 0;
};
} // namespace Renderer
