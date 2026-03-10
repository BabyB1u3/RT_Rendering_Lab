#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Buffers.h"

#include "core/Base.h"

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray &) = delete;
    VertexArray &operator=(const VertexArray &) = delete;

    VertexArray(VertexArray &&other) noexcept;
    VertexArray &operator=(VertexArray &&other) noexcept;

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer(const Ref<VertexBuffer> &vertexBuffer);
    void SetIndexBuffer(const Ref<IndexBuffer> &indexBuffer);

    const std::vector<Ref<VertexBuffer>> &GetVertexBuffers() const { return m_VertexBuffers; }
    const Ref<IndexBuffer> &GetIndexBuffer() const { return m_IndexBuffer; }

    uint32_t GetRendererID() const { return m_RendererID; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_VertexAttribIndex = 0;

    std::vector<Ref<VertexBuffer>> m_VertexBuffers;
    Ref<IndexBuffer> m_IndexBuffer;
};