#pragma once

/// @file GLRenderTarget.h
/// @brief OpenGL implementation of IRenderTarget.
///
/// Wraps either the default back buffer or an IFramebuffer into a unified target.

#include <cstdint>

#include "core/Base.h"
#include "graphics/interface/IFramebuffer.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/ITexture2D.h"

class GLRenderTarget : public IRenderTarget
{
public:
	~GLRenderTarget() override = default;

	GLRenderTarget(const GLRenderTarget &) = delete;
	GLRenderTarget &operator=(const GLRenderTarget &) = delete;

	GLRenderTarget(GLRenderTarget &&) noexcept = default;
	GLRenderTarget &operator=(GLRenderTarget &&) noexcept = default;

	// Factory methods (used by GLGraphicsDevice)
	static Ref<GLRenderTarget> CreateBackBuffer(uint32_t width, uint32_t height);
	static Ref<GLRenderTarget> CreateFromFramebuffer(const Ref<IFramebuffer> &fb);

	// --- IRenderTarget interface ---
	void Resize(uint32_t width, uint32_t height) override;

	uint32_t GetWidth() const override;
	uint32_t GetHeight() const override;

	bool IsBackBuffer() const override { return m_IsBackBuffer; }
	Ref<IFramebuffer> GetFramebuffer() const override { return m_Framebuffer; }

	Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const override;
	Ref<ITexture2D> GetDepthAttachment() const override;

private:
	GLRenderTarget() = default;

	bool m_IsBackBuffer = true;
	Ref<IFramebuffer> m_Framebuffer;

	// Only used by BackBuffer type
	uint32_t m_Width = 0;
	uint32_t m_Height = 0;
};
