#pragma once

/// @file GLTexture2D.h
/// @brief OpenGL implementation of ITexture2D using DSA.

#include <cstdint>
#include <string>

#include "core/Base.h"
#include "graphics/Texture.h"
#include "graphics/interface/ITexture2D.h"

class GLTexture2D : public ITexture2D
{
public:
	~GLTexture2D() override;

	GLTexture2D(const GLTexture2D &) = delete;
	GLTexture2D &operator=(const GLTexture2D &) = delete;

	GLTexture2D(GLTexture2D &&other) noexcept;
	GLTexture2D &operator=(GLTexture2D &&other) noexcept;

	// --- ITexture2D interface ---
	uint32_t GetWidth() const override { return m_Spec.Width; }
	uint32_t GetHeight() const override { return m_Spec.Height; }
	TextureFormat GetFormat() const override { return m_Spec.Format; }

	void Bind(uint32_t slot = 0) const override;
	void Unbind(uint32_t slot = 0) const override;

	void SetData(const void *data) override;

	bool operator==(const ITexture2D &other) const override;

	// --- GL-specific (non-virtual) ---
	uint32_t GetRendererID() const { return m_RendererID; }
	const TextureSpecification &GetSpecification() const { return m_Spec; }
	const std::string &GetPath() const { return m_Path; }

	// Factory (used by GLGraphicsDevice)
	static Ref<GLTexture2D> Create(const TextureSpecification &spec);
	static Ref<GLTexture2D> CreateFromFile(const std::string &path, bool flipVertically = true);

private:
	GLTexture2D(uint32_t rendererID, const TextureSpecification &spec, std::string path = {});

private:
	uint32_t m_RendererID = 0;
	TextureSpecification m_Spec;
	std::string m_Path;
};
