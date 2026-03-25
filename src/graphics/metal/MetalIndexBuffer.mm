#include "MetalIndexBuffer.h"

#import <Metal/Metal.h>

#include <stdexcept>

#include "core/Logger.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/metal/MetalGraphicsDevice.h"

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct MetalIndexBuffer::Impl
{
	id<MTLBuffer> buffer;
};

// ─── Construction ─────────────────────────────────────────────────────────────

MetalIndexBuffer::MetalIndexBuffer(const uint32_t *indices, uint32_t count)
	: m_Impl(std::make_unique<Impl>()), m_Count(count)
{
	auto *dev = static_cast<MetalGraphicsDevice *>(GetDevice().get());
	id<MTLDevice> device = (__bridge id<MTLDevice>)dev->GetMTLDevice();

	m_Impl->buffer = [device newBufferWithBytes:indices
	                                    length:count * sizeof(uint32_t)
	                                   options:MTLResourceStorageModeManaged];
	if (!m_Impl->buffer)
		throw std::runtime_error("MetalIndexBuffer: failed to create MTLBuffer");
}

MetalIndexBuffer::~MetalIndexBuffer() = default;

// ─── Metal-internal ───────────────────────────────────────────────────────────

void *MetalIndexBuffer::GetMTLBuffer() const
{
	return (__bridge void *)m_Impl->buffer;
}
