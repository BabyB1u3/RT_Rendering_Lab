#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Buffers.h"
#include "VertexArray.h"

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

    const std::shared_ptr<VertexArray> &GetVertexArray() const { return m_VertexArray; }
    const std::shared_ptr<IndexBuffer> &GetIndexBuffer() const { return m_IndexBuffer; }

    uint32_t GetIndexCount() const { return m_IndexCount; }

private:
    std::shared_ptr<VertexArray> m_VertexArray;
    std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
    std::shared_ptr<IndexBuffer> m_IndexBuffer;
    uint32_t m_IndexCount = 0;
};