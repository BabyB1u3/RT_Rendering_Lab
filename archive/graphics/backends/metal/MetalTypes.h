#pragma once

/// @file MetalTypes.h
/// @brief Format/enum conversion helpers between engine types and Metal API types.
///
/// Include ONLY from .mm (Objective-C++) translation units.
/// Never include from plain .cpp or from public .h headers.

#import <Metal/Metal.h>

#include "graphics/Buffers.h"
#include "graphics/Texture.h"
#include "graphics/RenderTypes.h"

// ─── Pixel formats ────────────────────────────────────────────────────────────

inline MTLPixelFormat PixelFormatFromTextureFormat(TextureFormat fmt)
{
	switch (fmt)
	{
	case TextureFormat::R8:              return MTLPixelFormatR8Unorm;
	case TextureFormat::RGB8:            return MTLPixelFormatRGBA8Unorm; // Metal has no RGB8; pad to RGBA8
	case TextureFormat::RGBA8:           return MTLPixelFormatRGBA8Unorm;
	case TextureFormat::RedInteger:      return MTLPixelFormatR32Sint;
	case TextureFormat::Depth:           return MTLPixelFormatDepth32Float;
	case TextureFormat::Depth24Stencil8: return MTLPixelFormatDepth32Float_Stencil8; // Metal lacks D24S8
	default:                             return MTLPixelFormatInvalid;
	}
}

inline bool IsDepthFormat(TextureFormat fmt)
{
	return fmt == TextureFormat::Depth || fmt == TextureFormat::Depth24Stencil8;
}

inline bool IsStencilFormat(TextureFormat fmt)
{
	return fmt == TextureFormat::Depth24Stencil8;
}

inline uint32_t BytesPerPixelMetal(TextureFormat fmt)
{
	switch (fmt)
	{
	case TextureFormat::R8:              return 1;
	case TextureFormat::RGB8:            return 4; // padded to RGBA
	case TextureFormat::RGBA8:           return 4;
	case TextureFormat::RedInteger:      return 4;
	case TextureFormat::Depth:           return 4;
	case TextureFormat::Depth24Stencil8: return 8;
	default:                             return 4;
	}
}

// ─── Vertex formats ───────────────────────────────────────────────────────────

inline MTLVertexFormat VertexFormatFromShaderDataType(ShaderDataType type)
{
	switch (type)
	{
	case ShaderDataType::Float:  return MTLVertexFormatFloat;
	case ShaderDataType::Float2: return MTLVertexFormatFloat2;
	case ShaderDataType::Float3: return MTLVertexFormatFloat3;
	case ShaderDataType::Float4: return MTLVertexFormatFloat4;
	case ShaderDataType::Int:    return MTLVertexFormatInt;
	case ShaderDataType::Int2:   return MTLVertexFormatInt2;
	case ShaderDataType::Int3:   return MTLVertexFormatInt3;
	case ShaderDataType::Int4:   return MTLVertexFormatInt4;
	// Mat3/Mat4 are expanded into multiple Float3/Float4 attributes by the caller.
	case ShaderDataType::Mat3:   return MTLVertexFormatFloat3;
	case ShaderDataType::Mat4:   return MTLVertexFormatFloat4;
	default:                     return MTLVertexFormatInvalid;
	}
}

// ─── Texture sampler ──────────────────────────────────────────────────────────

inline MTLSamplerAddressMode SamplerAddressModeFromWrap(TextureWrap wrap)
{
	switch (wrap)
	{
	case TextureWrap::Repeat:         return MTLSamplerAddressModeRepeat;
	case TextureWrap::ClampToEdge:    return MTLSamplerAddressModeClampToEdge;
	case TextureWrap::MirroredRepeat: return MTLSamplerAddressModeMirrorRepeat;
	default:                          return MTLSamplerAddressModeRepeat;
	}
}

inline MTLSamplerMinMagFilter SamplerMinMagFromFilter(TextureFilter f)
{
	return (f == TextureFilter::Nearest) ? MTLSamplerMinMagFilterNearest
	                                     : MTLSamplerMinMagFilterLinear;
}

inline MTLSamplerMipFilter SamplerMipFromFilter(TextureFilter f)
{
	return (f == TextureFilter::LinearMipmapLinear) ? MTLSamplerMipFilterLinear
	                                                 : MTLSamplerMipFilterNotMipmapped;
}

// ─── Render pass load/store actions ──────────────────────────────────────────

inline MTLLoadAction LoadActionToMTL(LoadAction action)
{
	switch (action)
	{
	case LoadAction::Load:     return MTLLoadActionLoad;
	case LoadAction::Clear:    return MTLLoadActionClear;
	case LoadAction::DontCare: return MTLLoadActionDontCare;
	}
	return MTLLoadActionDontCare;
}

inline MTLStoreAction StoreActionToMTL(StoreAction action)
{
	switch (action)
	{
	case StoreAction::Store:    return MTLStoreActionStore;
	case StoreAction::DontCare: return MTLStoreActionDontCare;
	}
	return MTLStoreActionDontCare;
}
