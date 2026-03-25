#include "MetalTexture2D.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <stb_image.h>
#include <glm/glm.hpp>

#include "core/Logger.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/metal/MetalGraphicsDevice.h"
#include "graphics/metal/MetalTypes.h"

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct MetalTexture2D::Impl
{
	id<MTLTexture>      texture;
	id<MTLSamplerState> sampler;
};

// ─── Internal helpers ─────────────────────────────────────────────────────────

static id<MTLDevice> GetMTLDevice()
{
	return (__bridge id<MTLDevice>)
	    static_cast<MetalGraphicsDevice *>(GetDevice().get())->GetMTLDevice();
}

static id<MTLSamplerState> CreateSampler(id<MTLDevice> device, const TextureSpecification &spec)
{
	MTLSamplerDescriptor *desc = [MTLSamplerDescriptor new];
	desc.sAddressMode  = SamplerAddressModeFromWrap(spec.WrapS);
	desc.tAddressMode  = SamplerAddressModeFromWrap(spec.WrapT);
	desc.minFilter     = SamplerMinMagFromFilter(spec.MinFilter);
	desc.magFilter     = SamplerMinMagFromFilter(spec.MagFilter);
	desc.mipFilter     = SamplerMipFromFilter(spec.MinFilter);
	return [device newSamplerStateWithDescriptor:desc];
}

static id<MTLTexture> AllocateTexture(id<MTLDevice> device, const TextureSpecification &spec)
{
	MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
	desc.pixelFormat = PixelFormatFromTextureFormat(spec.Format);
	desc.width       = spec.Width;
	desc.height      = spec.Height;
	desc.mipmapLevelCount = spec.GenerateMips
	    ? static_cast<NSUInteger>(std::floor(std::log2(std::max(spec.Width, spec.Height)))) + 1
	    : 1;
	desc.storageMode = MTLStorageModePrivate; // GPU-only; uploads go through blit encoder
	desc.usage       = MTLTextureUsageShaderRead;

	if (IsDepthFormat(spec.Format))
		desc.usage |= MTLTextureUsageRenderTarget;

	return [device newTextureWithDescriptor:desc];
}

/// Upload CPU data to a private MTLTexture via a blit command encoder.
static void UploadTextureData(id<MTLDevice>  device,
                              id<MTLTexture> texture,
                              const void    *data,
                              uint32_t       width,
                              uint32_t       height,
                              uint32_t       bytesPerRow)
{
	id<MTLCommandQueue>  queue   = (__bridge id<MTLCommandQueue>)
	    static_cast<MetalGraphicsDevice *>(GetDevice().get())->GetMTLCommandQueue();
	id<MTLCommandBuffer> cmd     = [queue commandBuffer];
	id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];

	// Staging buffer (shared memory so CPU can write)
	NSUInteger stagingSize = bytesPerRow * height;
	id<MTLBuffer> staging = [device newBufferWithBytes:data
	                                           length:stagingSize
	                                          options:MTLResourceStorageModeShared];

	[blit copyFromBuffer:staging
	        sourceOffset:0
	   sourceBytesPerRow:bytesPerRow
	 sourceBytesPerImage:stagingSize
	          sourceSize:MTLSizeMake(width, height, 1)
	           toTexture:texture
	    destinationSlice:0
	    destinationLevel:0
	   destinationOrigin:MTLOriginMake(0, 0, 0)];

	[blit endEncoding];
	[cmd commit];
	[cmd waitUntilCompleted]; // synchronous for correctness; async in Phase 2
}

// ─── Factories ────────────────────────────────────────────────────────────────

Ref<MetalTexture2D> MetalTexture2D::Create(const TextureSpecification &spec)
{
	auto *tex    = new MetalTexture2D();
	tex->m_Spec  = spec;
	tex->m_Impl  = std::make_unique<Impl>();

	id<MTLDevice> device = GetMTLDevice();
	tex->m_Impl->texture = AllocateTexture(device, spec);
	if (!tex->m_Impl->texture)
		throw std::runtime_error("MetalTexture2D: failed to create MTLTexture");

	tex->m_Impl->sampler = CreateSampler(device, spec);
	return Ref<MetalTexture2D>(tex);
}

Ref<MetalTexture2D> MetalTexture2D::CreateRenderTarget(const TextureSpecification &spec)
{
	auto *tex    = new MetalTexture2D();
	tex->m_Spec  = spec;
	tex->m_Impl  = std::make_unique<Impl>();

	id<MTLDevice> device = GetMTLDevice();

	MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
	desc.pixelFormat  = PixelFormatFromTextureFormat(spec.Format);
	desc.width        = spec.Width;
	desc.height       = spec.Height;
	desc.storageMode  = MTLStorageModePrivate;
	desc.usage        = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

	tex->m_Impl->texture = [device newTextureWithDescriptor:desc];
	if (!tex->m_Impl->texture)
		throw std::runtime_error("MetalTexture2D::CreateRenderTarget: failed to create MTLTexture");

	tex->m_Impl->sampler = CreateSampler(device, spec);
	return Ref<MetalTexture2D>(tex);
}

Ref<MetalTexture2D> MetalTexture2D::CreateFromFile(const std::string &path, bool flipVertically)
{
	stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

	int width, height, channels;
	stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
	if (!pixels)
		throw std::runtime_error("MetalTexture2D: stbi_load failed: " + path);

	TextureSpecification spec;
	spec.Width  = static_cast<uint32_t>(width);
	spec.Height = static_cast<uint32_t>(height);
	spec.GenerateMips = true;

	if (channels == 1)      spec.Format = TextureFormat::R8;
	else if (channels == 3) spec.Format = TextureFormat::RGB8;  // padded to RGBA on upload
	else                    spec.Format = TextureFormat::RGBA8;

	auto tex    = Create(spec);  // allocates MTLTexture
	tex->m_Path = path;
	tex->SetData(pixels);        // uploads (RGB→RGBA conversion happens here)

	stbi_image_free(pixels);
	return tex;
}

// ─── ITexture2D ───────────────────────────────────────────────────────────────

MetalTexture2D::~MetalTexture2D() = default;

void MetalTexture2D::SetData(const void *data)
{
	id<MTLDevice>  device  = GetMTLDevice();
	id<MTLTexture> texture = m_Impl->texture;

	if (m_Spec.Format == TextureFormat::RGB8)
	{
		// Metal has no native RGB8 — expand to RGBA8.
		uint32_t pixelCount = m_Spec.Width * m_Spec.Height;
		std::vector<uint8_t> rgba(pixelCount * 4);
		const uint8_t *src = static_cast<const uint8_t *>(data);
		for (uint32_t i = 0; i < pixelCount; ++i)
		{
			rgba[i * 4 + 0] = src[i * 3 + 0];
			rgba[i * 4 + 1] = src[i * 3 + 1];
			rgba[i * 4 + 2] = src[i * 3 + 2];
			rgba[i * 4 + 3] = 255;
		}
		UploadTextureData(device, texture, rgba.data(),
		                  m_Spec.Width, m_Spec.Height, m_Spec.Width * 4);
	}
	else
	{
		uint32_t bpp = BytesPerPixelMetal(m_Spec.Format);
		UploadTextureData(device, texture, data,
		                  m_Spec.Width, m_Spec.Height, m_Spec.Width * bpp);
	}

	// Generate mipmaps if requested.
	if (m_Spec.GenerateMips && texture.mipmapLevelCount > 1)
	{
		id<MTLCommandQueue>  queue = (__bridge id<MTLCommandQueue>)
		    static_cast<MetalGraphicsDevice *>(GetDevice().get())->GetMTLCommandQueue();
		id<MTLCommandBuffer> cmd  = [queue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
		[blit generateMipmapsForTexture:texture];
		[blit endEncoding];
		[cmd commit];
		[cmd waitUntilCompleted];
	}
}

bool MetalTexture2D::operator==(const ITexture2D &other) const
{
	const auto *o = dynamic_cast<const MetalTexture2D *>(&other);
	return o && m_Impl->texture == o->m_Impl->texture;
}

// ─── Metal-internal ───────────────────────────────────────────────────────────

void *MetalTexture2D::GetMTLTexture() const
{
	return (__bridge void *)m_Impl->texture;
}

void *MetalTexture2D::GetMTLSamplerState() const
{
	return (__bridge void *)m_Impl->sampler;
}
