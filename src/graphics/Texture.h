#pragma once

/// @file Texture.h
/// @brief GPU texture abstractions (DSA).
///
/// TextureFormat enumerates all supported internal formats (color, integer, depth).
/// Texture2D handles creation, uploading, and binding via DSA
/// (glCreateTextures / glTextureStorage2D / glBindTextureUnit).
///
/// Two creation paths:
///   - Create(spec):          empty GPU texture from a TextureSpecification.
///   - CreateFromFile(path):  load an image via stb_image and upload to GPU.
///
/// Textures are bound to numbered texture units via Bind(slot), matching
/// the sampler uniform index in the shader (e.g., TextureSlot::Albedo = 1).

#include <cstdint>
#include <memory>
#include <string>

#include "core/Base.h"

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

/// Abstract base class for all texture types.
class Texture
{
public:
	virtual ~Texture() = default;

	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual uint32_t GetRendererID() const = 0;
	virtual TextureFormat GetFormat() const = 0;

	virtual void Bind(uint32_t slot = 0) const = 0;
	virtual void Unbind(uint32_t slot = 0) const = 0;

	virtual bool operator==(const Texture &other) const = 0;
};

/// Concrete 2D texture backed by an OpenGL texture object (DSA).
class Texture2D : public Texture
{
public:
	~Texture2D() override;

	Texture2D(const Texture2D &) = delete;
	Texture2D &operator=(const Texture2D &) = delete;

	Texture2D(Texture2D &&other) noexcept;
	Texture2D &operator=(Texture2D &&other) noexcept;

	// Create from image file
	static Ref<Texture2D> CreateFromFile(
		const std::string &path,
		bool flipVertically = true);

	// Create an empty GPU texture from specification
	static Ref<Texture2D> Create(
		const TextureSpecification &spec);

	// Upload data to the whole texture
	// Intended mainly for ordinary color textures, not depth attachments
	void SetData(const void *data);

	uint32_t GetWidth() const override { return m_Spec.Width; }
	uint32_t GetHeight() const override { return m_Spec.Height; }
	uint32_t GetRendererID() const override { return m_RendererID; }
	TextureFormat GetFormat() const override { return m_Spec.Format; }

	const TextureSpecification &GetSpecification() const { return m_Spec; }
	const std::string &GetPath() const { return m_Path; }

	void Bind(uint32_t slot = 0) const override;
	void Unbind(uint32_t slot = 0) const override;

	bool operator==(const Texture &other) const override
	{
		return m_RendererID == other.GetRendererID();
	}

private:
	Texture2D(uint32_t rendererID, const TextureSpecification &spec, std::string path = {});

private:
	uint32_t m_RendererID = 0;
	TextureSpecification m_Spec;
	std::string m_Path;
};