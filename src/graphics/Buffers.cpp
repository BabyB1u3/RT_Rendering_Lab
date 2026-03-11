#include "Buffers.h"

#include <utility>

// -------------------- VertexBuffer --------------------

VertexBuffer::VertexBuffer(uint32_t size, BufferUsage usage)
	: m_Usage(usage)
{
	glCreateBuffers(1, &m_RendererID);
	glNamedBufferData(m_RendererID, size, nullptr, ToOpenGLBufferUsage(usage));
}

VertexBuffer::VertexBuffer(const void *data, uint32_t size, BufferUsage usage)
	: m_Usage(usage)
{
	glCreateBuffers(1, &m_RendererID);
	glNamedBufferData(m_RendererID, size, data, ToOpenGLBufferUsage(usage));
}

VertexBuffer::~VertexBuffer()
{
	if (m_RendererID != 0)
		glDeleteBuffers(1, &m_RendererID);
}

VertexBuffer::VertexBuffer(VertexBuffer &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Layout(std::move(other.m_Layout)),
	  m_Usage(other.m_Usage)
{
	other.m_RendererID = 0;
}

VertexBuffer &VertexBuffer::operator=(VertexBuffer &&other) noexcept
{
	if (this == &other)
		return *this;

	if (m_RendererID != 0)
		glDeleteBuffers(1, &m_RendererID);

	m_RendererID = other.m_RendererID;
	m_Layout = std::move(other.m_Layout);
	m_Usage = other.m_Usage;

	other.m_RendererID = 0;
	return *this;
}

void VertexBuffer::SetData(const void *data, uint32_t size, uint32_t offset)
{
	glNamedBufferSubData(m_RendererID, offset, size, data);
}

// -------------------- IndexBuffer --------------------

IndexBuffer::IndexBuffer(const uint32_t *indices, uint32_t count)
	: m_Count(count)
{
	glCreateBuffers(1, &m_RendererID);
	glNamedBufferData(m_RendererID, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}

IndexBuffer::~IndexBuffer()
{
	if (m_RendererID != 0)
		glDeleteBuffers(1, &m_RendererID);
}

IndexBuffer::IndexBuffer(IndexBuffer &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Count(other.m_Count)
{
	other.m_RendererID = 0;
	other.m_Count = 0;
}

IndexBuffer &IndexBuffer::operator=(IndexBuffer &&other) noexcept
{
	if (this == &other)
		return *this;

	if (m_RendererID != 0)
		glDeleteBuffers(1, &m_RendererID);

	m_RendererID = other.m_RendererID;
	m_Count = other.m_Count;

	other.m_RendererID = 0;
	other.m_Count = 0;
	return *this;
}

