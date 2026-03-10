#include "Mesh.h"

#include <cassert>

Mesh::Mesh(const void *vertexData,
           uint32_t vertexBufferSize,
           const BufferLayout &layout,
           const uint32_t *indices,
           uint32_t indexCount,
           BufferUsage vertexUsage)
    : m_IndexCount(indexCount)
{
    assert(vertexData != nullptr && "Mesh vertex data is null");
    assert(vertexBufferSize > 0 && "Mesh vertex buffer size must be > 0");
    assert(indices != nullptr && "Mesh index data is null");
    assert(indexCount > 0 && "Mesh index count must be > 0");
    assert(!layout.GetElements().empty() && "Mesh layout is empty");

    m_VertexArray = CreateRef<VertexArray>();

    auto vertexBuffer = CreateRef<VertexBuffer>(vertexData, vertexBufferSize, vertexUsage);
    vertexBuffer->SetLayout(layout);

    m_VertexArray->AddVertexBuffer(vertexBuffer);

    m_IndexBuffer = CreateRef<IndexBuffer>(indices, indexCount);
    m_VertexArray->SetIndexBuffer(m_IndexBuffer);

    m_VertexBuffers.push_back(vertexBuffer);
}

void Mesh::Bind() const
{
    m_VertexArray->Bind();
}

void Mesh::Unbind() const
{
    m_VertexArray->Unbind();
}