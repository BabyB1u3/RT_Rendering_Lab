#include "GLVertexBuffer.h"

#include <utility>

#include <glad/glad.h>

static GLenum ToOpenGLBufferUsage(BufferUsage usage)
{
	switch (usage)
	{
	case BufferUsage::StaticDraw:
		return GL_STATIC_DRAW;
	case BufferUsage::DynamicDraw:
		return GL_DYNAMIC_DRAW;
	case BufferUsage::StreamDraw:
		return GL_STREAM_DRAW;
	}
	return GL_STATIC_DRAW;
}

GLVertexBuffer::GLVertexBuffer(uint32_t size, BufferUsage usage)
	: m_Usage(usage)
{
	glCreateBuffers(1, &m_RendererID);
	glNamedBufferData(m_RendererID, size, nullptr, ToOpenGLBufferUsage(usage));
}

GLVertexBuffer::GLVertexBuffer(const void *data, uint32_t size, BufferUsage usage)
	: m_Usage(usage)
{
	glCreateBuffers(1, &m_RendererID);
	glNamedBufferData(m_RendererID, size, data, ToOpenGLBufferUsage(usage));
}

GLVertexBuffer::~GLVertexBuffer()
{
	if (m_RendererID != 0)
		glDeleteBuffers(1, &m_RendererID);
}

GLVertexBuffer::GLVertexBuffer(GLVertexBuffer &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Layout(std::move(other.m_Layout)),
	  m_Usage(other.m_Usage)
{
	other.m_RendererID = 0;
}

GLVertexBuffer &GLVertexBuffer::operator=(GLVertexBuffer &&other) noexcept
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

void GLVertexBuffer::SetData(const void *data, uint32_t size, uint32_t offset)
{
	glNamedBufferSubData(m_RendererID, offset, size, data);
}
