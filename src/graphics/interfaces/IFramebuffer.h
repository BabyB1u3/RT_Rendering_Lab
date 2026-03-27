#pragma once

/// @file IFramebuffer.h
/// @brief Abstract interface for framebuffer objects. Backend-agnostic.

#include <cstdint>

#include "core/Base.h"

class ITexture2D;
struct FramebufferSpecification;

class IFramebuffer
{
public:
	virtual ~IFramebuffer() = default;

	virtual void Bind() const = 0;
	virtual void Unbind() const = 0;
	virtual void Resize(uint32_t width, uint32_t height) = 0;

	virtual const FramebufferSpecification &GetSpecification() const = 0;

	virtual Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const = 0;
	virtual Ref<ITexture2D> GetDepthAttachment() const = 0;

	virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) const = 0;
	virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;
};
