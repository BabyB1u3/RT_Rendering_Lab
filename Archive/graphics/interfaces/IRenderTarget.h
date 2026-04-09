#pragma once

/// @file IRenderTarget.h
/// @brief Abstract interface for render targets. Backend-agnostic.
///
/// A render target represents either the back buffer (swap chain) or an off-screen
/// framebuffer. Render passes do NOT call Bind/Unbind on targets directly - instead,
/// they pass the target to IRenderCommand::BeginRenderPass() which handles binding
/// internally per backend.

#include <cstdint>

#include "core/Base.h"

class IFramebuffer;
class ITexture2D;

class IRenderTarget
{
public:
	virtual ~IRenderTarget() = default;

	virtual void Resize(uint32_t width, uint32_t height) = 0;

	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;

	virtual bool IsBackBuffer() const = 0;

	/// Returns the underlying framebuffer, or nullptr for back buffer targets.
	/// Used internally by backend render commands to bind the correct FBO / create
	/// the correct render pass descriptor.
	virtual Ref<IFramebuffer> GetFramebuffer() const = 0;

	virtual Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const = 0;
	virtual Ref<ITexture2D> GetDepthAttachment() const = 0;
};
