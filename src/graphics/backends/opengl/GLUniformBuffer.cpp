#include "GLUniformBuffer.h"

#include <utility>

#include <glad/glad.h>

#include "core/diagnostics/Assert.h"

GLUniformBuffer::GLUniformBuffer(uint32_t size)
    : m_Size(size)
{
    glCreateBuffers(1, &m_RendererID);
    glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
}

GLUniformBuffer::~GLUniformBuffer()
{
    if (m_RendererID != 0)
        glDeleteBuffers(1, &m_RendererID);
}

GLUniformBuffer::GLUniformBuffer(GLUniformBuffer &&other) noexcept
    : m_RendererID(other.m_RendererID), m_Size(other.m_Size)
{
    other.m_RendererID = 0;
    other.m_Size = 0;
}

GLUniformBuffer &GLUniformBuffer::operator=(GLUniformBuffer &&other) noexcept
{
    if (this == &other)
        return *this;

    if (m_RendererID != 0)
        glDeleteBuffers(1, &m_RendererID);

    m_RendererID = other.m_RendererID;
    m_Size = other.m_Size;

    other.m_RendererID = 0;
    other.m_Size = 0;
    return *this;
}

void GLUniformBuffer::SetData(const void *data, uint32_t size, uint32_t offset)
{
    RTRLAB_ASSERT_MSG(offset + size <= m_Size, "GLUniformBuffer::SetData: write exceeds allocated buffer size");
    glNamedBufferSubData(m_RendererID, offset, size, data);
}
