#include "GLTexture2D.h"

#include <utility>

#include <glad/glad.h>
#include <stb_image.h>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/ErrorMacros.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"

static GLenum TextureWrapToGL(TextureWrap wrap)
{
	switch (wrap)
	{
	case TextureWrap::Repeat:
		return GL_REPEAT;
	case TextureWrap::ClampToEdge:
		return GL_CLAMP_TO_EDGE;
	case TextureWrap::MirroredRepeat:
		return GL_MIRRORED_REPEAT;
	}
	return GL_REPEAT;
}

static GLenum TextureFilterToGL(TextureFilter filter)
{
	switch (filter)
	{
	case TextureFilter::Nearest:
		return GL_NEAREST;
	case TextureFilter::Linear:
		return GL_LINEAR;
	case TextureFilter::LinearMipmapLinear:
		return GL_LINEAR_MIPMAP_LINEAR;
	}
	return GL_LINEAR;
}

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

GLTexture2D::GLTexture2D(uint32_t rendererID, const TextureSpecification &spec, std::string path)
	: m_RendererID(rendererID), m_Spec(spec), m_Path(std::move(path))
{
}

GLTexture2D::~GLTexture2D()
{
	if (m_RendererID != 0)
		glDeleteTextures(1, &m_RendererID);
}

GLTexture2D::GLTexture2D(GLTexture2D &&other) noexcept
	: m_RendererID(other.m_RendererID),
	  m_Spec(other.m_Spec),
	  m_Path(std::move(other.m_Path))
{
	other.m_RendererID = 0;
}

GLTexture2D &GLTexture2D::operator=(GLTexture2D &&other) noexcept
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

Ref<GLTexture2D> GLTexture2D::Create(const TextureSpecification &spec)
{
	uint32_t rendererID = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &rendererID);

	GLenum internalFormat = TextureFormatToGLInternalFormat(spec.Format);
	RTRLAB_ASSERT_MSG(internalFormat != 0, "Unsupported texture format");

	uint32_t mipLevels = 1;
	if (spec.GenerateMips)
	{
		uint32_t size = spec.Width > spec.Height ? spec.Width : spec.Height;
		while (size > 1)
		{
			size >>= 1;
			++mipLevels;
		}
	}

	glTextureStorage2D(rendererID, mipLevels, internalFormat, spec.Width, spec.Height);

	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_S, TextureWrapToGL(spec.WrapS));
	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_T, TextureWrapToGL(spec.WrapT));
	glTextureParameteri(rendererID, GL_TEXTURE_MIN_FILTER, TextureFilterToGL(spec.MinFilter));
	glTextureParameteri(rendererID, GL_TEXTURE_MAG_FILTER, TextureFilterToGL(spec.MagFilter));

	return Ref<GLTexture2D>(new GLTexture2D(rendererID, spec));
}

Ref<GLTexture2D> GLTexture2D::CreateFromFile(const std::string &path, bool flipVertically)
{
	stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

	int width = 0, height = 0, channels = 0;
	stbi_uc *data = stbi_load(path.c_str(), &width, &height, &channels, 0);
	if (!data)
	{
		LOG_ERROR_CAT(LogCategory::Graphics, "Failed to load texture: {}", path);
		return nullptr;
	}

	TextureSpecification spec;
	spec.Width = static_cast<uint32_t>(width);
	spec.Height = static_cast<uint32_t>(height);
	spec.GenerateMips = false;
	spec.WrapS = TextureWrap::Repeat;
	spec.WrapT = TextureWrap::Repeat;
	spec.MinFilter = TextureFilter::Linear;
	spec.MagFilter = TextureFilter::Linear;

	if (channels == 1)
		spec.Format = TextureFormat::R8;
	else if (channels == 3)
		spec.Format = TextureFormat::RGB8;
	else if (channels == 4)
		spec.Format = TextureFormat::RGBA8;
	else
	{
		stbi_image_free(data);
		LOG_ERROR_CAT(LogCategory::Graphics, "Unsupported channel count ({}) in texture: {}", channels, path);
		return nullptr;
	}

	uint32_t rendererID = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &rendererID);

	GLenum internalFormat = TextureFormatToGLInternalFormat(spec.Format);
	GLenum dataFormat = TextureFormatToGLDataFormat(spec.Format);
	GLenum dataType = TextureFormatToGLDataType(spec.Format);

	glTextureStorage2D(rendererID, 1, internalFormat, spec.Width, spec.Height);
	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_S, TextureWrapToGL(spec.WrapS));
	glTextureParameteri(rendererID, GL_TEXTURE_WRAP_T, TextureWrapToGL(spec.WrapT));
	glTextureParameteri(rendererID, GL_TEXTURE_MIN_FILTER, TextureFilterToGL(spec.MinFilter));
	glTextureParameteri(rendererID, GL_TEXTURE_MAG_FILTER, TextureFilterToGL(spec.MagFilter));

	glTextureSubImage2D(rendererID, 0, 0, 0, spec.Width, spec.Height, dataFormat, dataType, data);

	if (spec.GenerateMips)
		glGenerateTextureMipmap(rendererID);

	stbi_image_free(data);

	LOG_TRACE_CAT(LogCategory::Graphics, "Texture loaded: {} ({}x{}, {} channels)", path, width, height, channels);
	return Ref<GLTexture2D>(new GLTexture2D(rendererID, spec, path));
}

void GLTexture2D::SetData(const void *data)
{
	ERR_FAIL_COND_MSG_CAT(LogCategory::Graphics, data == nullptr, "GLTexture2D::SetData received null data");

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
		ERR_FAIL_COND_MSG_CAT(LogCategory::Graphics, true,
							  "GLTexture2D::SetData only supports ordinary color textures");
	}

	(void)bpp; // used for validation only

	glTextureSubImage2D(
		m_RendererID, 0, 0, 0,
		m_Spec.Width, m_Spec.Height,
		dataFormat, dataType, data);
}

void GLTexture2D::Bind(uint32_t slot) const
{
	glBindTextureUnit(slot, m_RendererID);
}

void GLTexture2D::Unbind(uint32_t slot) const
{
	glBindTextureUnit(slot, 0);
}

bool GLTexture2D::operator==(const ITexture2D &other) const
{
	// Compare by GL renderer ID within the GL backend
	const auto *glOther = dynamic_cast<const GLTexture2D *>(&other);
	if (!glOther)
		return false;
	return m_RendererID == glOther->m_RendererID;
}
