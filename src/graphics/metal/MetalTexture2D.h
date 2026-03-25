#pragma once

/// @file MetalTexture2D.h
/// @brief Metal implementation of ITexture2D — wraps a MTLTexture + MTLSamplerState.

#include <cstdint>
#include <memory>
#include <string>

#include "core/Base.h"
#include "graphics/Texture.h"
#include "graphics/interface/ITexture2D.h"

class MetalTexture2D : public ITexture2D
{
public:
	~MetalTexture2D() override;

	MetalTexture2D(const MetalTexture2D &) = delete;
	MetalTexture2D &operator=(const MetalTexture2D &) = delete;

	// --- ITexture2D ---
	uint32_t GetWidth()  const override { return m_Spec.Width; }
	uint32_t GetHeight() const override { return m_Spec.Height; }
	TextureFormat GetFormat() const override { return m_Spec.Format; }

	/// No-op in Metal — binding is driven by MetalRenderCommand::SetTexture.
	void Bind(uint32_t /*slot*/ = 0) const override {}
	void Unbind(uint32_t /*slot*/ = 0) const override {}

	void SetData(const void *data) override;

	bool operator==(const ITexture2D &other) const override;

	// --- Metal-internal (call only from .mm files) ---
	void *GetMTLTexture()      const; // id<MTLTexture>
	void *GetMTLSamplerState() const; // id<MTLSamplerState>

	const TextureSpecification &GetSpecification() const { return m_Spec; }

	// Factories (used by MetalGraphicsDevice)
	static Ref<MetalTexture2D> Create(const TextureSpecification &spec);
	static Ref<MetalTexture2D> CreateFromFile(const std::string &path, bool flipVertically = true);
	/// Creates with MTLTextureUsageRenderTarget | ShaderRead — for framebuffer attachments.
	static Ref<MetalTexture2D> CreateRenderTarget(const TextureSpecification &spec);

private:
	MetalTexture2D() = default;

	struct Impl;
	std::unique_ptr<Impl> m_Impl;

	TextureSpecification m_Spec;
	std::string          m_Path;
};
