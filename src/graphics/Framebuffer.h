#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Base.h"
#include "Texture.h"

// Framebuffer attachment specification
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

// Framebuffer specification
struct FramebufferSpecification
{
	uint32_t Width = 0;
	uint32_t Height = 0;

	FramebufferAttachmentSpecification Attachments;

	uint32_t Samples = 1;
	bool SwapChainTarget = false;
};

// Framebuffer
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

	void Resize(uint32_t width, uint32_t height);

	int ReadPixel(uint32_t attachmentIndex, int x, int y) const;
	void ClearAttachment(uint32_t attachmentIndex, int value);

	uint32_t GetRendererID() const { return m_RendererID; }

	const FramebufferSpecification &GetSpecification() const { return m_Specification; }
	std::size_t GetColorAttachmentCount() const { return m_ColorAttachments.size(); }

	Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const;
	Ref<Texture2D> GetDepthAttachment() const;

private:
	void Invalidate();

private:
	uint32_t m_RendererID = 0;
	FramebufferSpecification m_Specification;

	std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
	FramebufferTextureSpecification m_DepthAttachmentSpecification;

	std::vector<Ref<Texture2D>> m_ColorAttachments;
	Ref<Texture2D> m_DepthAttachment;
};
