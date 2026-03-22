#pragma once

/// @file ITexture2D.h
/// @brief Abstract interface for 2D textures. Backend-agnostic.

#include <cstdint>

#include "core/Base.h"
#include "graphics/Texture.h" // TextureFormat

class ITexture2D
{
public:
	virtual ~ITexture2D() = default;

	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual TextureFormat GetFormat() const = 0;

	virtual void Bind(uint32_t slot = 0) const = 0;
	virtual void Unbind(uint32_t slot = 0) const = 0;

	virtual void SetData(const void *data) = 0;

	virtual bool operator==(const ITexture2D &other) const = 0;
};
