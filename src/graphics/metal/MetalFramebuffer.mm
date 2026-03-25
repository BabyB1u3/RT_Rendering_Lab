#include "MetalFramebuffer.h"

#import <Metal/Metal.h>

#include <stdexcept>

#include "core/Logger.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/metal/MetalGraphicsDevice.h"
#include "graphics/metal/MetalTexture2D.h"
#include "graphics/metal/MetalTypes.h"

// ─── Construction ─────────────────────────────────────────────────────────────

MetalFramebuffer::MetalFramebuffer(const FramebufferSpecification &spec)
	: m_Spec(spec)
{
	Invalidate();
}

MetalFramebuffer::~MetalFramebuffer() = default;

// ─── IFramebuffer ─────────────────────────────────────────────────────────────

void MetalFramebuffer::Resize(uint32_t width, uint32_t height)
{
	if (m_Spec.Width == width && m_Spec.Height == height)
		return;

	m_Spec.Width  = width;
	m_Spec.Height = height;
	Invalidate();
}

Ref<ITexture2D> MetalFramebuffer::GetColorAttachment(uint32_t index) const
{
	if (index >= m_ColorAttachments.size())
		return nullptr;
	return m_ColorAttachments[index];
}

Ref<ITexture2D> MetalFramebuffer::GetDepthAttachment() const
{
	return m_DepthAttachment;
}

int MetalFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y) const
{
	if (attachmentIndex >= m_ColorAttachments.size())
		return -1;

	auto *metalTex      = static_cast<MetalTexture2D *>(m_ColorAttachments[attachmentIndex].get());
	id<MTLTexture> tex  = (__bridge id<MTLTexture>)metalTex->GetMTLTexture();
	auto *dev           = static_cast<MetalGraphicsDevice *>(GetDevice().get());
	id<MTLDevice>       device = (__bridge id<MTLDevice>)dev->GetMTLDevice();
	id<MTLCommandQueue> queue  = (__bridge id<MTLCommandQueue>)dev->GetMTLCommandQueue();

	uint32_t bpp     = BytesPerPixelMetal(m_ColorAttachments[attachmentIndex]->GetFormat());
	id<MTLBuffer> staging = [device newBufferWithLength:bpp
	                                            options:MTLResourceStorageModeShared];

	id<MTLCommandBuffer>      cmd  = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];

	[blit copyFromTexture:tex
	          sourceSlice:0
	          sourceLevel:0
	         sourceOrigin:MTLOriginMake(static_cast<NSUInteger>(x), static_cast<NSUInteger>(y), 0)
	           sourceSize:MTLSizeMake(1, 1, 1)
	             toBuffer:staging
	    destinationOffset:0
	destinationBytesPerRow:bpp
	destinationBytesPerImage:bpp];

	[blit endEncoding];
	[cmd commit];
	[cmd waitUntilCompleted]; // GPU sync required for CPU readback

	int result = 0;
	memcpy(&result, staging.contents, std::min(bpp, 4u));
	return result;
}

void MetalFramebuffer::ClearAttachment(uint32_t /*attachmentIndex*/, int /*value*/)
{
	// Clearing is done through MTLRenderPassDescriptor load actions in BeginRenderPass.
	LOG_WARN("MetalFramebuffer::ClearAttachment: not implemented (use RenderPassDescriptor clear instead)");
}

// ─── Private ──────────────────────────────────────────────────────────────────

void MetalFramebuffer::Invalidate()
{
	m_ColorAttachments.clear();
	m_DepthAttachment = nullptr;

	for (const auto &attachSpec : m_Spec.Attachments.Attachments)
	{
		TextureSpecification texSpec;
		texSpec.Width  = m_Spec.Width;
		texSpec.Height = m_Spec.Height;
		texSpec.Format = attachSpec.Format;

		if (IsDepthFormat(attachSpec.Format))
			m_DepthAttachment = MetalTexture2D::CreateRenderTarget(texSpec);
		else
			m_ColorAttachments.push_back(MetalTexture2D::CreateRenderTarget(texSpec));
	}

	LOG_TRACE("MetalFramebuffer: invalidated ({}x{})", m_Spec.Width, m_Spec.Height);
}
