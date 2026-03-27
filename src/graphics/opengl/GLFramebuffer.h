#pragma once

/// @file GLFramebuffer.h
/// @brief OpenGL framebuffer object (FBO) implementation of IFramebuffer using DSA.

#include <cstdint>
#include <vector>

#include "core/Base.h"
#include "graphics/Framebuffer.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/ITexture2D.h"

class GLFramebuffer : public IFramebuffer
{
public:
	explicit GLFramebuffer(const FramebufferSpecification &spec);
	~GLFramebuffer() override;

	GLFramebuffer(const GLFramebuffer &) = delete;
	GLFramebuffer &operator=(const GLFramebuffer &) = delete;

	GLFramebuffer(GLFramebuffer &&other) noexcept;
	GLFramebuffer &operator=(GLFramebuffer &&other) noexcept;

	// --- IFramebuffer interface ---
	void Bind() const override;
	void Unbind() const override;
	void Resize(uint32_t width, uint32_t height) override;

	const FramebufferSpecification &GetSpecification() const override { return m_Specification; }

	Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const override;
	Ref<ITexture2D> GetDepthAttachment() const override;

	int ReadPixel(uint32_t attachmentIndex, int x, int y) const override;
	void ClearAttachment(uint32_t attachmentIndex, int value) override;

	// --- GL-specific (non-virtual) ---
	uint32_t GetRendererID() const { return m_RendererID; }
	std::size_t GetColorAttachmentCount() const { return m_ColorAttachments.size(); }

private:
	void Invalidate();

private:
	uint32_t m_RendererID = 0;
	FramebufferSpecification m_Specification;

	std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
	FramebufferTextureSpecification m_DepthAttachmentSpecification;

	std::vector<Ref<ITexture2D>> m_ColorAttachments;
	Ref<ITexture2D> m_DepthAttachment;
};
