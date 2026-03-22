#pragma once

/// @file Mesh.h
/// @brief GPU-ready mesh: a VAO + vertex/index buffers, created from raw vertex data.
///
/// A Mesh packages a VertexArray, one or more VertexBuffers, and an IndexBuffer
/// into a single drawable unit. It does NOT include material or transform data —
/// those are stored separately in RenderItem.
///
/// Meshes are typically created via MeshFactory (cube, plane, sphere, quad)
/// rather than constructed directly.

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Base.h"
#include "Buffers.h"

class IVertexArray;
class IVertexBuffer;
class IIndexBuffer;

class Mesh
{
public:
    Mesh(const void *vertexData,
         uint32_t vertexBufferSize,
         const BufferLayout &layout,
         const uint32_t *indices,
         uint32_t indexCount,
         BufferUsage vertexUsage = BufferUsage::StaticDraw);

    ~Mesh() = default;

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    Mesh(Mesh &&) noexcept = default;
    Mesh &operator=(Mesh &&) noexcept = default;

    void Bind() const;
    void Unbind() const;

    const Ref<IVertexArray> &GetVertexArray() const { return m_VertexArray; }
    const Ref<IIndexBuffer> &GetIndexBuffer() const { return m_IndexBuffer; }

    uint32_t GetIndexCount() const { return m_IndexCount; }

private:
    Ref<IVertexArray> m_VertexArray;
    // Could have multiple vertexbuffers in the future:
    // Main vertex buffer + intance buffer + secondary stream
    std::vector<Ref<IVertexBuffer>> m_VertexBuffers;
    Ref<IIndexBuffer> m_IndexBuffer;
    uint32_t m_IndexCount = 0;
};