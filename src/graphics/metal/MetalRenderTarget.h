#pragma once

/// @file MetalRenderTarget.h
/// @brief Metal implementation of IRenderTarget.
///
/// Two modes:
///   Back buffer  — IsBackBuffer() == true, GetFramebuffer() == nullptr.
///                  MetalRenderCommand uses the current CAMetalDrawable texture.
///   FBO          — wraps a MetalFramebuffer. MetalRenderCommand reads its
///                  attachment textures to build the MTLRenderPassDescriptor.

#include <cstdint>

#include "core/Base.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/ITexture2D.h"

class MetalRenderTarget : public IRenderTarget
{
public:
	~MetalRenderTarget() override = default;

	MetalRenderTarget(const MetalRenderTarget &) = delete;
	MetalRenderTarget &operator=(const MetalRenderTarget &) = delete;

	// Factories (used by MetalGraphicsDevice)
	static Ref<MetalRenderTarget> CreateBackBuffer(uint32_t width, uint32_t height);
	static Ref<MetalRenderTarget> CreateFromFramebuffer(const Ref<IFramebuffer> &fb);

	// --- IRenderTarget ---
	void Resize(uint32_t width, uint32_t height) override;

	uint32_t GetWidth()  const override;
	uint32_t GetHeight() const override;

	bool IsBackBuffer() const override { return m_IsBackBuffer; }
	Ref<IFramebuffer> GetFramebuffer() const override { return m_Framebuffer; }

	Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const override;
	Ref<ITexture2D> GetDepthAttachment() const override;

private:
	MetalRenderTarget() = default;

	bool              m_IsBackBuffer = true;
	Ref<IFramebuffer> m_Framebuffer;

	uint32_t m_Width  = 0;
	uint32_t m_Height = 0;
};
