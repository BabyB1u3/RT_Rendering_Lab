#pragma once

/// @file Framebuffer.h
/// @brief Off-screen framebuffer object (FBO) abstraction using DSA.
///
/// A Framebuffer owns its color and depth Texture2D attachments. It is created
/// from a FramebufferSpecification that lists the desired attachment formats.
///
/// Construction / Resize flow:
///   1. Separate the spec's attachments into color vs. depth lists.
///   2. Invalidate(): delete old FBO & textures, create new ones, attach, and
///      verify completeness (GL_FRAMEBUFFER_COMPLETE).
///
/// Depth-only FBOs (e.g., shadow maps) are supported: if no color attachments
/// are specified, draw/read buffers are set to GL_NONE.
///
/// See also: RenderTarget — a thin renderer-level wrapper that unifies
/// Framebuffer targets and the default back buffer under one interface.

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Base.h"
#include "Texture.h"

/// Describes a single FBO attachment (color or depth) by its texture format.
struct FramebufferTextureSpecification
{
	TextureFormat Format = TextureFormat::None;

	FramebufferTextureSpecification() = default;
	FramebufferTextureSpecification(TextureFormat format)
		: Format(format) {}
};

struct FramebufferAttachmentSpecification
{
	std::vector<FramebufferTextureSpecification> Attachments;

	FramebufferAttachmentSpecification() = default;
	FramebufferAttachmentSpecification(
		std::initializer_list<FramebufferTextureSpecification> attachments)
		: Attachments(attachments) {}
};

/// Full FBO creation parameters: dimensions, attachments, sample count.
struct FramebufferSpecification
{
	uint32_t Width = 0;
	uint32_t Height = 0;

	FramebufferAttachmentSpecification Attachments;

	uint32_t Samples = 1;
	bool SwapChainTarget = false;
};

/// Off-screen framebuffer object. Owns its color/depth Texture2D attachments.
class Framebuffer
{
public:
	explicit Framebuffer(const FramebufferSpecification &spec);
	~Framebuffer();

	Framebuffer(const Framebuffer &) = delete;
	Framebuffer &operator=(const Framebuffer &) = delete;

	Framebuffer(Framebuffer &&other) noexcept;
	Framebuffer &operator=(Framebuffer &&other) noexcept;

	void Bind() const;
	void Unbind() const;

	/// Resize all attachments. Triggers a full Invalidate() (destroy + recreate).
	void Resize(uint32_t width, uint32_t height);

	/// Read a single pixel from an integer-format color attachment (e.g., entity ID picking).
	int ReadPixel(uint32_t attachmentIndex, int x, int y) const;
	/// Clear an integer-format color attachment to a uniform value.
	void ClearAttachment(uint32_t attachmentIndex, int value);

	uint32_t GetRendererID() const { return m_RendererID; }

	const FramebufferSpecification &GetSpecification() const { return m_Specification; }
	std::size_t GetColorAttachmentCount() const { return m_ColorAttachments.size(); }

	Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const;
	Ref<Texture2D> GetDepthAttachment() const;

private:
	/// Destroy and recreate the FBO and all attachments from the current specification.
	void Invalidate();

private:
	uint32_t m_RendererID = 0;
	FramebufferSpecification m_Specification;

	std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
	FramebufferTextureSpecification m_DepthAttachmentSpecification;

	std::vector<Ref<Texture2D>> m_ColorAttachments;
	Ref<Texture2D> m_DepthAttachment;
};
