#pragma once

/// @file IRenderTarget.h
/// @brief Abstract interface for render targets. Backend-agnostic.
/// @see docs/renderTarget_design.md §13.3 for design rationale.

#include <cstdint>

#include "core/Base.h"

class ITexture2D;

class IRenderTarget
{
public:
	virtual ~IRenderTarget() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;

	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;

	virtual bool IsBackBuffer() const = 0;

	virtual Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const = 0;
	virtual Ref<ITexture2D> GetDepthAttachment() const = 0;
};
