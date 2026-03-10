#include "Framebuffer.h"

#include <cassert>
#include <utility>

namespace
{
	constexpr uint32_t s_MaxFramebufferSize = 8192;

	static bool IsDepthFormat(TextureFormat format)
	{
		return format == TextureFormat::Depth ||
			   format == TextureFormat::Depth24Stencil8;
	}
}

Framebuffer::Framebuffer(const FramebufferSpecification &spec)
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

Framebuffer::~Framebuffer()
{
	if (m_RendererID != 0)
		glDeleteFramebuffers(1, &m_RendererID);
}

Framebuffer::Framebuffer(Framebuffer &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Specification(other.m_Specification),
	  m_ColorAttachmentSpecifications(std::move(other.m_ColorAttachmentSpecifications)),
	  m_DepthAttachmentSpecification(other.m_DepthAttachmentSpecification),
	  m_ColorAttachments(std::move(other.m_ColorAttachments)),
	  m_DepthAttachment(std::move(other.m_DepthAttachment))
{
	other.m_RendererID = 0;
}

Framebuffer &Framebuffer::operator=(Framebuffer &&other) noexcept
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

void Framebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	glViewport(0, 0,
			   static_cast<GLsizei>(m_Specification.Width),
			   static_cast<GLsizei>(m_Specification.Height));
}

void Framebuffer::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize)
		return;

	m_Specification.Width = width;
	m_Specification.Height = height;
	Invalidate();
}

Ref<Texture2D> Framebuffer::GetColorAttachment(uint32_t index) const
{
	assert(index < m_ColorAttachments.size() && "Color attachment index out of range");
	return m_ColorAttachments[index];
}

Ref<Texture2D> Framebuffer::GetDepthAttachment() const
{
	return m_DepthAttachment;
}

int Framebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) const
{
	assert(attachmentIndex < m_ColorAttachments.size() && "Attachment index out of range");
	glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);

	int pixelData = 0;
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
	return pixelData;
}

void Framebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
{
	assert(attachmentIndex < m_ColorAttachments.size() && "Attachment index out of range");
	glClearTexImage(
		m_ColorAttachments[attachmentIndex]->GetRendererID(),
		0,
		GL_RED_INTEGER,
		GL_INT,
		&value);
}

void Framebuffer::Invalidate()
{
	if (m_RendererID != 0)
	{
		glDeleteFramebuffers(1, &m_RendererID);
		m_RendererID = 0;

		m_ColorAttachments.clear();
		m_DepthAttachment.reset();
	}

	glCreateFramebuffers(1, &m_RendererID);
	glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

	// Color attachments
	for (uint32_t i = 0; i < m_ColorAttachmentSpecifications.size(); ++i)
	{
		TextureSpecification spec;
		spec.Width = m_Specification.Width;
		spec.Height = m_Specification.Height;
		spec.Format = m_ColorAttachmentSpecifications[i].Format;
		spec.WrapS = GL_CLAMP_TO_EDGE;
		spec.WrapT = GL_CLAMP_TO_EDGE;
		spec.MinFilter = GL_LINEAR;
		spec.MagFilter = GL_LINEAR;

		auto texture = Texture2D::Create(spec);
		m_ColorAttachments.push_back(texture);

		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0 + i,
			GL_TEXTURE_2D,
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
		spec.WrapS = GL_CLAMP_TO_EDGE;
		spec.WrapT = GL_CLAMP_TO_EDGE;
		spec.MinFilter = GL_NEAREST;
		spec.MagFilter = GL_NEAREST;

		m_DepthAttachment = Texture2D::Create(spec);

		GLenum attachmentType =
			(m_DepthAttachmentSpecification.Format == TextureFormat::Depth24Stencil8)
				? GL_DEPTH_STENCIL_ATTACHMENT
				: GL_DEPTH_ATTACHMENT;

		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			attachmentType,
			GL_TEXTURE_2D,
			m_DepthAttachment->GetRendererID(),
			0);
	}

	if (m_ColorAttachments.size() > 1)
	{
		assert(m_ColorAttachments.size() <= 4 && "Too many color attachments");

		GLenum buffers[4] = {
			GL_COLOR_ATTACHMENT0,
			GL_COLOR_ATTACHMENT1,
			GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3};
		glDrawBuffers(static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
	}
	else if (m_ColorAttachments.empty())
	{
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}

	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE &&
		   "Framebuffer is incomplete");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}