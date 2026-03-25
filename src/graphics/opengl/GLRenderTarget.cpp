#include "GLRenderTarget.h"

#include <cassert>

#include "graphics/Framebuffer.h" // FramebufferSpecification (full definition)

Ref<GLRenderTarget> GLRenderTarget::CreateBackBuffer(uint32_t width, uint32_t height)
{
	auto target = Ref<GLRenderTarget>(new GLRenderTarget());
	target->m_IsBackBuffer = true;
	target->m_Width = width;
	target->m_Height = height;
	return target;
}

Ref<GLRenderTarget> GLRenderTarget::CreateFromFramebuffer(const Ref<IFramebuffer> &fb)
{
	assert(fb && "GLRenderTarget::CreateFromFramebuffer requires a valid framebuffer");

	auto target = Ref<GLRenderTarget>(new GLRenderTarget());
	target->m_IsBackBuffer = false;
	target->m_Framebuffer = fb;
	return target;
}

void GLRenderTarget::Resize(uint32_t width, uint32_t height)
{
	if (m_IsBackBuffer)
	{
		m_Width = width;
		m_Height = height;
	}
	else
	{
		assert(m_Framebuffer && "Framebuffer target is null");
		m_Framebuffer->Resize(width, height);
	}
}

uint32_t GLRenderTarget::GetWidth() const
{
	if (!m_IsBackBuffer)
	{
		assert(m_Framebuffer && "Framebuffer target is null");
		return m_Framebuffer->GetSpecification().Width;
	}
	return m_Width;
}

uint32_t GLRenderTarget::GetHeight() const
{
	if (!m_IsBackBuffer)
	{
		assert(m_Framebuffer && "Framebuffer target is null");
		return m_Framebuffer->GetSpecification().Height;
	}
	return m_Height;
}

Ref<ITexture2D> GLRenderTarget::GetColorAttachment(uint32_t index) const
{
	if (!m_IsBackBuffer)
	{
		assert(m_Framebuffer && "Framebuffer target is null");
		return m_Framebuffer->GetColorAttachment(index);
	}
	return nullptr;
}

Ref<ITexture2D> GLRenderTarget::GetDepthAttachment() const
{
	if (!m_IsBackBuffer)
	{
		assert(m_Framebuffer && "Framebuffer target is null");
		return m_Framebuffer->GetDepthAttachment();
	}
	return nullptr;
}
