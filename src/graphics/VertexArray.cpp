#include "VertexArray.h"

#include <cassert>
#include <utility>

static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
{
	switch (type)
	{
	case ShaderDataType::Float:
	case ShaderDataType::Float2:
	case ShaderDataType::Float3:
	case ShaderDataType::Float4:
	case ShaderDataType::Mat3:
	case ShaderDataType::Mat4:
		return GL_FLOAT;

	case ShaderDataType::Int:
	case ShaderDataType::Int2:
	case ShaderDataType::Int3:
	case ShaderDataType::Int4:
		return GL_INT;

	case ShaderDataType::Bool:
		return GL_BOOL;

	case ShaderDataType::None:
		return 0;
	}

	assert(false && "Unknown ShaderDataType");
	return 0;
}

VertexArray::VertexArray()
{
	glCreateVertexArrays(1, &m_RendererID);
}

VertexArray::~VertexArray()
{
	if (m_RendererID != 0)
		glDeleteVertexArrays(1, &m_RendererID);
}

VertexArray::VertexArray(VertexArray &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_VertexAttribIndex(other.m_VertexAttribIndex),
	  m_VertexBuffers(std::move(other.m_VertexBuffers)),
	  m_IndexBuffer(std::move(other.m_IndexBuffer))
{
	other.m_RendererID = 0;
	other.m_VertexAttribIndex = 0;
}

VertexArray &VertexArray::operator=(VertexArray &&other) noexcept
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

void VertexArray::Bind() const
{
	glBindVertexArray(m_RendererID);
}

void VertexArray::Unbind() const
{
	glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(const Ref<VertexBuffer> &vertexBuffer)
{
	const auto &layout = vertexBuffer->GetLayout();
	assert(!layout.GetElements().empty() && "VertexBuffer has no layout!");

	Bind();
	vertexBuffer->Bind();

	for (const auto &element : layout)
	{
		switch (element.Type)
		{
		case ShaderDataType::Float:
		case ShaderDataType::Float2:
		case ShaderDataType::Float3:
		case ShaderDataType::Float4:
		{
			glEnableVertexAttribArray(m_VertexAttribIndex);
			glVertexAttribPointer(
				m_VertexAttribIndex,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				layout.GetStride(),
				reinterpret_cast<const void *>(element.Offset));
			glVertexAttribDivisor(m_VertexAttribIndex, element.Divisor);
			++m_VertexAttribIndex;
			break;
		}

		case ShaderDataType::Int:
		case ShaderDataType::Int2:
		case ShaderDataType::Int3:
		case ShaderDataType::Int4:
		case ShaderDataType::Bool:
		{
			glEnableVertexAttribArray(m_VertexAttribIndex);
			glVertexAttribIPointer(
				m_VertexAttribIndex,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type),
				layout.GetStride(),
				reinterpret_cast<const void *>(element.Offset));
			glVertexAttribDivisor(m_VertexAttribIndex, element.Divisor);
			++m_VertexAttribIndex;
			break;
		}

		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
		{
			uint32_t count = element.GetComponentCount(); // 3 or 4
			for (uint32_t i = 0; i < count; ++i)
			{
				glEnableVertexAttribArray(m_VertexAttribIndex);
				glVertexAttribPointer(
					m_VertexAttribIndex,
					count,
					ShaderDataTypeToOpenGLBaseType(element.Type),
					element.Normalized ? GL_TRUE : GL_FALSE,
					layout.GetStride(),
					reinterpret_cast<const void *>(element.Offset + sizeof(float) * count * i));
				glVertexAttribDivisor(m_VertexAttribIndex, element.Divisor);
				++m_VertexAttribIndex;
			}
			break;
		}

		case ShaderDataType::None:
		default:
			assert(false && "Unknown ShaderDataType in VertexArray::AddVertexBuffer");
		}
	}

	m_VertexBuffers.push_back(vertexBuffer);
}

void VertexArray::SetIndexBuffer(const Ref<IndexBuffer> &indexBuffer)
{
	Bind();
	indexBuffer->Bind();
	m_IndexBuffer = indexBuffer;
}