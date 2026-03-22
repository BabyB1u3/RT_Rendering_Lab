#pragma once

/// @file Texture.h
/// @brief Texture enums, specification structs, and related constants.
///
/// Concrete texture classes live in the backend subdirectory (e.g. opengl/GLTexture2D).
/// Create textures via GetDevice()->CreateTexture2D() / CreateTexture2DFromFile().

#include <cstdint>

/// Supported GPU texture internal formats.
enum class TextureFormat
{
	None = 0,
	R8,
	RGB8,
	RGBA8,
	RedInteger, // integer attachments (e.g. entity ID picking)
	Depth,
	Depth24Stencil8
};

/// Backend-agnostic texture wrap mode.
enum class TextureWrap
{
	Repeat,
	ClampToEdge,
	MirroredRepeat
};

/// Backend-agnostic texture filter mode.
enum class TextureFilter
{
	Nearest,
	Linear,
	LinearMipmapLinear
};

// Texture creation spec
struct TextureSpecification
{
	uint32_t Width = 1;
	uint32_t Height = 1;
	TextureFormat Format = TextureFormat::RGBA8;

	bool GenerateMips = false;

	TextureWrap WrapS = TextureWrap::Repeat;
	TextureWrap WrapT = TextureWrap::Repeat;
	TextureFilter MinFilter = TextureFilter::Linear;
	TextureFilter MagFilter = TextureFilter::Linear;
};