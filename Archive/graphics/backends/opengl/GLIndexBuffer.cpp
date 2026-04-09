#include "GLIndexBuffer.h"

#include <utility>

#include <glad/glad.h>

GLIndexBuffer::GLIndexBuffer(const uint32_t *indices, uint32_t count)
	: m_Count(count)
{
	glCreateBuffers(1, &m_RendererID);
	glNamedBufferData(m_RendererID, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
}

GLIndexBuffer::~GLIndexBuffer()
{
	if (m_RendererID != 0)
		glDeleteBuffers(1, &m_RendererID);
}

GLIndexBuffer::GLIndexBuffer(GLIndexBuffer &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Count(other.m_Count)
{
	other.m_RendererID = 0;
	other.m_Count = 0;
}

GLIndexBuffer &GLIndexBuffer::operator=(GLIndexBuffer &&other) noexcept
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
