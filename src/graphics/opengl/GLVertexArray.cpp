#include "GLVertexArray.h"

#include <cassert>
#include <utility>

#include <glad/glad.h>

#include "graphics/Buffers.h"
#include "GLCast.h"
#include "GLVertexBuffer.h"
#include "GLIndexBuffer.h"

GLVertexArray::GLVertexArray()
{
	glCreateVertexArrays(1, &m_RendererID);
}

GLVertexArray::~GLVertexArray()
{
	if (m_RendererID != 0)
		glDeleteVertexArrays(1, &m_RendererID);
}

GLVertexArray::GLVertexArray(GLVertexArray &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_VertexAttribIndex(other.m_VertexAttribIndex),
	  m_VertexBuffers(std::move(other.m_VertexBuffers)),
	  m_IndexBuffer(std::move(other.m_IndexBuffer))
{
	other.m_RendererID = 0;
	other.m_VertexAttribIndex = 0;
}

GLVertexArray &GLVertexArray::operator=(GLVertexArray &&other) noexcept
{
	if (this == &other)
		return *this;

	if (m_RendererID != 0)
		glDeleteVertexArrays(1, &m_RendererID);

	m_RendererID = other.m_RendererID;
	m_VertexAttribIndex = other.m_VertexAttribIndex;
	m_VertexBuffers = std::move(other.m_VertexBuffers);
	m_IndexBuffer = std::move(other.m_IndexBuffer);

	other.m_RendererID = 0;
	other.m_VertexAttribIndex = 0;
	return *this;
}

void GLVertexArray::Bind() const
{
	glBindVertexArray(m_RendererID);
}

void GLVertexArray::Unbind() const
{
	glBindVertexArray(0);
}

void GLVertexArray::AddVertexBuffer(const Ref<IVertexBuffer> &vb)
{
	auto *glVB = AsGL<GLVertexBuffer>(vb);
	const auto &layout = glVB->GetLayout();
	assert(!layout.GetElements().empty() && "VertexBuffer has no layout!");

	GLuint bindingIndex = static_cast<GLuint>(m_VertexBuffers.size());
	glVertexArrayVertexBuffer(m_RendererID, bindingIndex, glVB->GetRendererID(), 0, layout.GetStride());

	for (const auto &element : layout)
	{
		switch (element.Type)
		{
		case ShaderDataType::Float:
		case ShaderDataType::Float2:
		case ShaderDataType::Float3:
		case ShaderDataType::Float4:
		{
			glEnableVertexArrayAttrib(m_RendererID, m_VertexAttribIndex);
			glVertexArrayAttribFormat(
				m_RendererID,
				m_VertexAttribIndex,
				element.GetComponentCount(),
				GL_FLOAT,
				element.Normalized ? GL_TRUE : GL_FALSE,
				static_cast<GLuint>(element.Offset));
			glVertexArrayAttribBinding(m_RendererID, m_VertexAttribIndex, bindingIndex);
			glVertexArrayBindingDivisor(m_RendererID, bindingIndex, element.Divisor);
			++m_VertexAttribIndex;
			break;
		}

		case ShaderDataType::Int:
		case ShaderDataType::Int2:
		case ShaderDataType::Int3:
		case ShaderDataType::Int4:
		case ShaderDataType::Bool:
		{
			glEnableVertexArrayAttrib(m_RendererID, m_VertexAttribIndex);
			glVertexArrayAttribIFormat(
				m_RendererID,
				m_VertexAttribIndex,
				element.GetComponentCount(),
				element.Type == ShaderDataType::Bool ? GL_BOOL : GL_INT,
				static_cast<GLuint>(element.Offset));
			glVertexArrayAttribBinding(m_RendererID, m_VertexAttribIndex, bindingIndex);
			glVertexArrayBindingDivisor(m_RendererID, bindingIndex, element.Divisor);
			++m_VertexAttribIndex;
			break;
		}

		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
		{
			uint32_t count = element.GetComponentCount();
			for (uint32_t i = 0; i < count; ++i)
			{
				glEnableVertexArrayAttrib(m_RendererID, m_VertexAttribIndex);
				glVertexArrayAttribFormat(
					m_RendererID,
					m_VertexAttribIndex,
					count,
					GL_FLOAT,
					element.Normalized ? GL_TRUE : GL_FALSE,
					static_cast<GLuint>(element.Offset + sizeof(float) * count * i));
				glVertexArrayAttribBinding(m_RendererID, m_VertexAttribIndex, bindingIndex);
				glVertexArrayBindingDivisor(m_RendererID, bindingIndex, element.Divisor);
				++m_VertexAttribIndex;
			}
			break;
		}

		case ShaderDataType::None:
		default:
			assert(false && "Unknown ShaderDataType in GLVertexArray::AddVertexBuffer");
		}
	}

	m_VertexBuffers.push_back(vb);
}

void GLVertexArray::SetIndexBuffer(const Ref<IIndexBuffer> &ib)
{
	auto *glIB = AsGL<GLIndexBuffer>(ib);
	glVertexArrayElementBuffer(m_RendererID, glIB->GetRendererID());
	m_IndexBuffer = ib;
}
