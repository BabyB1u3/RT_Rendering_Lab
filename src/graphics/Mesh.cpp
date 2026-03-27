#include "Mesh.h"

#include "core/diagnostics/Assert.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/interface/IIndexBuffer.h"
#include "graphics/interface/IVertexArray.h"
#include "graphics/interface/IVertexBuffer.h"

Mesh::Mesh(const void *vertexData,
           uint32_t vertexBufferSize,
           const BufferLayout &layout,
           const uint32_t *indices,
           uint32_t indexCount,
           BufferUsage vertexUsage)
    : m_IndexCount(indexCount)
{
    RTRLAB_ASSERT_MSG(vertexData != nullptr, "Mesh vertex data is null");
    RTRLAB_ASSERT_MSG(vertexBufferSize > 0, "Mesh vertex buffer size must be > 0");
    RTRLAB_ASSERT_MSG(indices != nullptr, "Mesh index data is null");
    RTRLAB_ASSERT_MSG(indexCount > 0, "Mesh index count must be > 0");
    RTRLAB_ASSERT_MSG(!layout.GetElements().empty(), "Mesh layout is empty");

    auto device = GetDevice();

    m_VertexArray = device->CreateVertexArray();

    auto vertexBuffer = device->CreateVertexBuffer(vertexData, vertexBufferSize, vertexUsage);
    vertexBuffer->SetLayout(layout);

    m_VertexArray->AddVertexBuffer(vertexBuffer);

    m_IndexBuffer = device->CreateIndexBuffer(indices, indexCount);
    m_VertexArray->SetIndexBuffer(m_IndexBuffer);

    m_VertexBuffers.push_back(vertexBuffer);
}

void Mesh::Bind() const
{
    if (m_VertexArray)
        m_VertexArray->Bind();
}

void Mesh::Unbind() const
{
    if (m_VertexArray)
        m_VertexArray->Unbind();
}
