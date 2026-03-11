#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <glad/glad.h>

#include "core/Base.h"

// Texture format
enum class TextureFormat
{
	None = 0,
	R8,
	RGB8,
	RGBA8,
	Depth,
	Depth24Stencil8
};

// Texture creation spec
struct TextureSpecification
{
	uint32_t Width = 1;
	uint32_t Height = 1;
	TextureFormat Format = TextureFormat::RGBA8;

	bool GenerateMips = false;

	GLenum WrapS = GL_REPEAT;
	GLenum WrapT = GL_REPEAT;
	GLenum MinFilter = GL_LINEAR;
	GLenum MagFilter = GL_LINEAR;
};

// Abstract texture base
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

// 2D texture
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
	void SetData(const void *data, uint32_t size);

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