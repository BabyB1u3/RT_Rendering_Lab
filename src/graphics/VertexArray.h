#pragma once

/// @file VertexArray.h
/// @brief OpenGL Vertex Array Object (VAO) abstraction using DSA.
///
/// A VertexArray binds together one or more VertexBuffers (with their layouts)
/// and an IndexBuffer. When AddVertexBuffer() is called, it configures all
/// vertex attribute pointers via DSA (glVertexArrayAttribFormat, etc.) so that
/// no global VAO bind is needed during setup.
///
/// Matrix attributes (Mat3, Mat4) are automatically expanded into multiple
/// consecutive vec3/vec4 attribute slots.

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

    /// Attach a vertex buffer and configure its attribute pointers from the buffer's layout.
    /// Each call increments the internal binding index, so multiple VBOs are supported.
    void AddVertexBuffer(const Ref<VertexBuffer> &vertexBuffer);
    /// Set the element/index buffer used for indexed drawing.
    void SetIndexBuffer(const Ref<IndexBuffer> &indexBuffer);

    const std::vector<Ref<VertexBuffer>> &GetVertexBuffers() const { return m_VertexBuffers; }
    const Ref<IndexBuffer> &GetIndexBuffer() const { return m_IndexBuffer; }

    uint32_t GetRendererID() const { return m_RendererID; }

private:
    uint32_t m_RendererID = 0;
    /// Running attribute index — incremented by AddVertexBuffer for each element.
    uint32_t m_VertexAttribIndex = 0;

    std::vector<Ref<VertexBuffer>> m_VertexBuffers;
    Ref<IndexBuffer> m_IndexBuffer;
};