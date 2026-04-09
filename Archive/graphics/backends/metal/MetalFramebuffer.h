#pragma once

/// @file MetalFramebuffer.h
/// @brief Metal implementation of IFramebuffer - a set of MTLTexture attachments.
///
/// MetalRenderCommand::BeginRenderPass() reads the color/depth attachments
/// via GetColorAttachment() / GetDepthAttachment() to build the
/// MTLRenderPassDescriptor. No Metal types leak into this header.

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Base.h"
#include "graphics/Framebuffer.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/ITexture2D.h"

class MetalFramebuffer : public IFramebuffer
{
public:
	explicit MetalFramebuffer(const FramebufferSpecification &spec);
	~MetalFramebuffer() override;

	MetalFramebuffer(const MetalFramebuffer &) = delete;
	MetalFramebuffer &operator=(const MetalFramebuffer &) = delete;

	// --- IFramebuffer ---
	/// No-op for Metal - binding is driven by MetalRenderCommand::BeginRenderPass.
	void Bind() const override {}
	void Unbind() const override {}

	void Resize(uint32_t width, uint32_t height) override;

	const FramebufferSpecification &GetSpecification() const override { return m_Spec; }

	Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const override;
	Ref<ITexture2D> GetDepthAttachment() const override;

	int ReadPixel(uint32_t attachmentIndex, int x, int y) const override;
	void ClearAttachment(uint32_t attachmentIndex, int value) override;

private:
	void Invalidate();

private:
	FramebufferSpecification m_Spec;

	std::vector<Ref<ITexture2D>> m_ColorAttachments;
	Ref<ITexture2D> m_DepthAttachment;
};
