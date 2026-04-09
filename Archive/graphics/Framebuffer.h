#pragma once

/// @file Framebuffer.h
/// @brief Framebuffer specification structs and related types.
///
/// Concrete framebuffer classes live in the backend subdirectory (e.g. opengl/GLFramebuffer).
/// Create framebuffers via GetDevice()->CreateFramebuffer().

#include <cstdint>
#include <vector>

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

