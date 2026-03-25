#include "MetalRenderTarget.h"

#include "graphics/metal/MetalFramebuffer.h"

// ─── Factories ────────────────────────────────────────────────────────────────

Ref<MetalRenderTarget> MetalRenderTarget::CreateBackBuffer(uint32_t width, uint32_t height)
{
	auto *rt      = new MetalRenderTarget();
	rt->m_IsBackBuffer = true;
	rt->m_Width        = width;
	rt->m_Height       = height;
	return Ref<MetalRenderTarget>(rt);
}

Ref<MetalRenderTarget> MetalRenderTarget::CreateFromFramebuffer(const Ref<IFramebuffer> &fb)
{
	auto *rt      = new MetalRenderTarget();
	rt->m_IsBackBuffer = false;
	rt->m_Framebuffer  = fb;
	return Ref<MetalRenderTarget>(rt);
}

// ─── IRenderTarget ────────────────────────────────────────────────────────────

void MetalRenderTarget::Resize(uint32_t width, uint32_t height)
{
	m_Width  = width;
	m_Height = height;

	if (m_Framebuffer)
		m_Framebuffer->Resize(width, height);
}

uint32_t MetalRenderTarget::GetWidth() const
{
	if (m_Framebuffer)
		return m_Framebuffer->GetSpecification().Width;
	return m_Width;
}

uint32_t MetalRenderTarget::GetHeight() const
{
	if (m_Framebuffer)
		return m_Framebuffer->GetSpecification().Height;
	return m_Height;
}

Ref<ITexture2D> MetalRenderTarget::GetColorAttachment(uint32_t index) const
{
	if (m_Framebuffer)
		return m_Framebuffer->GetColorAttachment(index);
	return nullptr;
}

Ref<ITexture2D> MetalRenderTarget::GetDepthAttachment() const
{
	if (m_Framebuffer)
		return m_Framebuffer->GetDepthAttachment();
	return nullptr;
}
