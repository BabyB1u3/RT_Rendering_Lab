#include "GLFramebuffer.h"

#include <cassert>
#include <stdexcept>
#include <utility>

#include <glad/glad.h>

#include "core/Logger.h"
#include "GLCast.h"
#include "GLTexture2D.h"

namespace
{
	constexpr uint32_t s_MaxFramebufferSize = 8192;

	bool IsDepthFormat(TextureFormat format)
	{
		return format == TextureFormat::Depth ||
			   format == TextureFormat::Depth24Stencil8;
	}

	bool IsIntegerFormat(TextureFormat format)
	{
		return format == TextureFormat::RedInteger;
	}
}

GLFramebuffer::GLFramebuffer(const FramebufferSpecification &spec)
	: m_Specification(spec)
{
	for (const auto &attachment : m_Specification.Attachments.Attachments)
	{
		if (IsDepthFormat(attachment.Format))
			m_DepthAttachmentSpecification = attachment;
		else if (attachment.Format != TextureFormat::None)
			m_ColorAttachmentSpecifications.push_back(attachment);
	}

	Invalidate();
}

GLFramebuffer::~GLFramebuffer()
{
	if (m_RendererID != 0)
		glDeleteFramebuffers(1, &m_RendererID);
}

GLFramebuffer::GLFramebuffer(GLFramebuffer &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Specification(other.m_Specification),
	  m_ColorAttachmentSpecifications(std::move(other.m_ColorAttachmentSpecifications)),
	  m_DepthAttachmentSpecification(other.m_DepthAttachmentSpecification),
	  m_ColorAttachments(std::move(other.m_ColorAttachments)),
	  m_DepthAttachment(std::move(other.m_DepthAttachment))
{
	other.m_RendererID = 0;
}

GLFramebuffer &GLFramebuffer::operator=(GLFramebuffer &&other) noexcept
{
	if (this == &other)
		return *this;

	if (m_RendererID != 0)
		glDeleteFramebuffers(1, &m_RendererID);

	m_RendererID = other.m_RendererID;
	m_Specification = other.m_Specification;
	m_ColorAttachmentSpecifications = std::move(other.m_ColorAttachmentSpecifications);
	m_DepthAttachmentSpecification = other.m_DepthAttachmentSpecification;
	m_ColorAttachments = std::move(other.m_ColorAttachments);
	m_DepthAttachment = std::move(other.m_DepthAttachment);

	other.m_RendererID = 0;
	return *this;
}

void GLFramebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
}

void GLFramebuffer::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
		return;

	m_Specification.Width = width;
	m_Specification.Height = height;
	Invalidate();
}

Ref<ITexture2D> GLFramebuffer::GetColorAttachment(uint32_t index) const
{
	if (index >= m_ColorAttachments.size())
		return nullptr;
	return m_ColorAttachments[index];
}

Ref<ITexture2D> GLFramebuffer::GetDepthAttachment() const
{
	return m_DepthAttachment;
}

int GLFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) const
{
	assert(attachmentIndex < m_ColorAttachments.size() && "Attachment index out of range");
	assert(IsIntegerFormat(m_ColorAttachmentSpecifications[attachmentIndex].Format) &&
		   "ReadPixel requires an integer-format attachment");

	glNamedFramebufferReadBuffer(m_RendererID, GL_COLOR_ATTACHMENT0 + attachmentIndex);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
	int pixelData = 0;
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	return pixelData;
}

void GLFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
{
	assert(attachmentIndex < m_ColorAttachments.size() && "Attachment index out of range");
	assert(IsIntegerFormat(m_ColorAttachmentSpecifications[attachmentIndex].Format) &&
		   "ClearAttachment requires an integer-format attachment");

	auto *glTex = AsGL<GLTexture2D>(m_ColorAttachments[attachmentIndex]);
	glClearTexImage(glTex->GetRendererID(), 0, GL_RED_INTEGER, GL_INT, &value);
}

void GLFramebuffer::Invalidate()
{
	if (m_RendererID != 0)
	{
		glDeleteFramebuffers(1, &m_RendererID);
		m_RendererID = 0;

		m_ColorAttachments.clear();
		m_DepthAttachment.reset();
	}

	glCreateFramebuffers(1, &m_RendererID);

	// Color attachments
	for (uint32_t i = 0; i < m_ColorAttachmentSpecifications.size(); ++i)
	{
		TextureSpecification spec;
		spec.Width = m_Specification.Width;
		spec.Height = m_Specification.Height;
		spec.Format = m_ColorAttachmentSpecifications[i].Format;
		spec.WrapS = TextureWrap::ClampToEdge;
		spec.WrapT = TextureWrap::ClampToEdge;
		spec.MinFilter = TextureFilter::Linear;
		spec.MagFilter = TextureFilter::Linear;

		auto texture = GLTexture2D::Create(spec);
		m_ColorAttachments.push_back(texture);

		glNamedFramebufferTexture(
			m_RendererID,
			GL_COLOR_ATTACHMENT0 + i,
			texture->GetRendererID(),
			0);
	}

	// Depth attachment
	if (m_DepthAttachmentSpecification.Format != TextureFormat::None)
	{
		TextureSpecification spec;
		spec.Width = m_Specification.Width;
		spec.Height = m_Specification.Height;
		spec.Format = m_DepthAttachmentSpecification.Format;
		spec.WrapS = TextureWrap::ClampToEdge;
		spec.WrapT = TextureWrap::ClampToEdge;
		spec.MinFilter = TextureFilter::Nearest;
		spec.MagFilter = TextureFilter::Nearest;

		auto depthTex = GLTexture2D::Create(spec);
		m_DepthAttachment = depthTex;

		GLenum attachmentType =
			(m_DepthAttachmentSpecification.Format == TextureFormat::Depth24Stencil8)
				? GL_DEPTH_STENCIL_ATTACHMENT
				: GL_DEPTH_ATTACHMENT;

		glNamedFramebufferTexture(
			m_RendererID,
			attachmentType,
			depthTex->GetRendererID(),
			0);
	}

	if (!m_ColorAttachments.empty())
	{
		assert(m_ColorAttachments.size() <= 4 && "Too many color attachments");

		GLenum buffers[4] = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3};
		glNamedFramebufferDrawBuffers(m_RendererID, static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
	}
	else
	{
		glNamedFramebufferDrawBuffer(m_RendererID, GL_NONE);
		glNamedFramebufferReadBuffer(m_RendererID, GL_NONE);
	}

	GLenum fbStatus = glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER);
	if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		LOG_ERROR("GLFramebuffer incomplete: status = 0x{:X}", fbStatus);
		throw std::runtime_error("GLFramebuffer incomplete: status = " + std::to_string(fbStatus));
	}
}
