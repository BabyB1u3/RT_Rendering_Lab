#include "Texture.h"

#include <cassert>
#include <stdexcept>
#include <utility>

#include <stb_image.h>

static GLenum TextureFormatToGLInternalFormat(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::R8:
		return GL_R8;
	case TextureFormat::RGB8:
		return GL_RGB8;
	case TextureFormat::RGBA8:
		return GL_RGBA8;
	case TextureFormat::RedInteger:
		return GL_R32I;
	case TextureFormat::Depth:
		return GL_DEPTH_COMPONENT24;
	case TextureFormat::Depth24Stencil8:
		return GL_DEPTH24_STENCIL8;
	case TextureFormat::None:
		return 0;
	}
	return 0;
}

static GLenum TextureFormatToGLDataFormat(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::R8:
		return GL_RED;
	case TextureFormat::RGB8:
		return GL_RGB;
	case TextureFormat::RGBA8:
		return GL_RGBA;
	case TextureFormat::RedInteger:
		return GL_RED_INTEGER;
	case TextureFormat::Depth:
		return GL_DEPTH_COMPONENT;
	case TextureFormat::Depth24Stencil8:
		return GL_DEPTH_STENCIL;
	case TextureFormat::None:
		return 0;
	}
	return 0;
}

static GLenum TextureFormatToGLDataType(TextureFormat format)
{
	switch (format)
	{
	case TextureFormat::R8:
	case TextureFormat::RGB8:
	case TextureFormat::RGBA8:
		return GL_UNSIGNED_BYTE;
	case TextureFormat::RedInteger:
		return GL_INT;
	case TextureFormat::Depth:
		return GL_FLOAT;
	case TextureFormat::Depth24Stencil8:
		return GL_UNSIGNED_INT_24_8;
	case TextureFormat::None:
		return 0;
	}
	return 0;
}

Texture2D::Texture2D(uint32_t rendererID, const TextureSpecification &spec, std::string path)
	: m_RendererID(rendererID), m_Spec(spec), m_Path(std::move(path))
{
}

Texture2D::~Texture2D()
{
	if (m_RendererID != 0)
		glDeleteTextures(1, &m_RendererID);
}

Texture2D::Texture2D(Texture2D &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Spec(other.m_Spec),
	  m_Path(std::move(other.m_Path))
{
	other.m_RendererID = 0;
}

Texture2D &Texture2D::operator=(Texture2D &&other) noexcept
{
	if (this == &other)
		return *this;

	if (m_RendererID != 0)
		glDeleteTextures(1, &m_RendererID);

	m_RendererID = other.m_RendererID;
	m_Spec = other.m_Spec;
	m_Path = std::move(other.m_Path);

	other.m_RendererID = 0;
	return *this;
}

Ref<Texture2D> Texture2D::Create(const TextureSpecification &spec)
{
	uint32_t rendererID = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &rendererID);

	GLenum internalFormat = TextureFormatToGLInternalFormat(spec.Format);
	GLenum dataFormat = TextureFormatToGLDataFormat(spec.Format);
	GLenum dataType = TextureFormatToGLDataType(spec.Format);

	assert(internalFormat != 0 && "Unsupported texture format");

	uint32_t mipLevels = 1;
	if (spec.GenerateMips)
	{
		uint32_t size = spec.Width > spec.Height ? spec.Width : spec.Height;
		while (size > 1) { size >>= 1; ++mipLevels; }
	}

	glTextureStorage2D(rendererID, mipLevels, internalFormat, spec.Width, spec.Height);

	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_S, spec.WrapS);
	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_T, spec.WrapT);
	glTextureParameteri(rendererID, GL_TEXTURE_MIN_FILTER, spec.MinFilter);
	glTextureParameteri(rendererID, GL_TEXTURE_MAG_FILTER, spec.MagFilter);

	// Optional initialization for depth/depth-stencil/color textures
	if (dataFormat != 0 && dataType != 0 &&
		(spec.Format == TextureFormat::Depth || spec.Format == TextureFormat::Depth24Stencil8))
	{
		glTextureSubImage2D(rendererID, 0, 0, 0, spec.Width, spec.Height, dataFormat, dataType, nullptr);
	}

	if (spec.GenerateMips)
		glGenerateTextureMipmap(rendererID);

	return Ref<Texture2D>(new Texture2D(rendererID, spec));
}

Ref<Texture2D> Texture2D::CreateFromFile(const std::string &path, bool flipVertically)
{
	stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

	int width = 0, height = 0, channels = 0;
	stbi_uc *data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	if (!data)
		throw std::runtime_error("Failed to load texture: " + path);

	TextureSpecification spec;
	spec.Width = static_cast<uint32_t>(width);
	spec.Height = static_cast<uint32_t>(height);
	spec.GenerateMips = false;
	spec.WrapS = GL_REPEAT;
	spec.WrapT = GL_REPEAT;
	spec.MinFilter = GL_LINEAR;
	spec.MagFilter = GL_LINEAR;

	if (channels == 1)
		spec.Format = TextureFormat::R8;
	else if (channels == 3)
		spec.Format = TextureFormat::RGB8;
	else if (channels == 4)
		spec.Format = TextureFormat::RGBA8;
	else
	{
		stbi_image_free(data);
		throw std::runtime_error("Unsupported image channel count in texture: " + path);
	}

	uint32_t rendererID = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &rendererID);

	GLenum internalFormat = TextureFormatToGLInternalFormat(spec.Format);
	GLenum dataFormat = TextureFormatToGLDataFormat(spec.Format);
	GLenum dataType = TextureFormatToGLDataType(spec.Format);

	glTextureStorage2D(rendererID, 1, internalFormat, spec.Width, spec.Height);
	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_S, spec.WrapS);
	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_T, spec.WrapT);
	glTextureParameteri(rendererID, GL_TEXTURE_MIN_FILTER, spec.MinFilter);
	glTextureParameteri(rendererID, GL_TEXTURE_MAG_FILTER, spec.MagFilter);

	glTextureSubImage2D(rendererID, 0, 0, 0, spec.Width, spec.Height, dataFormat, dataType, data);

	if (spec.GenerateMips)
		glGenerateTextureMipmap(rendererID);

	stbi_image_free(data);

	return Ref<Texture2D>(new Texture2D(rendererID, spec, path));
}

void Texture2D::SetData(const void *data, uint32_t size)
{
	GLenum dataFormat = TextureFormatToGLDataFormat(m_Spec.Format);
	GLenum dataType = TextureFormatToGLDataType(m_Spec.Format);

	uint32_t bpp = 0;
	switch (m_Spec.Format)
	{
	case TextureFormat::R8:
		bpp = 1;
		break;
	case TextureFormat::RGB8:
		bpp = 3;
		break;
	case TextureFormat::RGBA8:
		bpp = 4;
		break;
	default:
		assert(false && "SetData only supports ordinary color textures");
		return;
	}

	assert(size == m_Spec.Width * m_Spec.Height * bpp && "Texture data size mismatch");

	glTextureSubImage2D(
		m_RendererID, 0, 0, 0,
		m_Spec.Width, m_Spec.Height,
		dataFormat, dataType, data);
}

void Texture2D::Bind(uint32_t slot) const
{
	glBindTextureUnit(slot, m_RendererID);
}

void Texture2D::Unbind(uint32_t slot) const
{
	glBindTextureUnit(slot, 0);
}