#include "MetalVertexBuffer.h"

#import <Metal/Metal.h>

#include <cstring>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/ErrorMacros.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/backends/metal/MetalGraphicsDevice.h"

// --- Impl ---

struct MetalVertexBuffer::Impl
{
	id<MTLBuffer> buffer;
};

// --- Helpers ---

static id<MTLDevice> GetMTLDeviceFromGlobal()
{
	auto *dev = static_cast<MetalGraphicsDevice *>(GetDevice().get());
	return (__bridge id<MTLDevice>)dev->GetMTLDevice();
}

// --- Construction ---

MetalVertexBuffer::MetalVertexBuffer(uint32_t size, BufferUsage usage)
	: m_Impl(std::make_unique<Impl>()), m_Usage(usage), m_AllocatedSize(size)
{
	id<MTLDevice> device = GetMTLDeviceFromGlobal();

	// Shared memory: CPU can write, GPU reads. Suitable for dynamic/stream data.
	m_Impl->buffer = [device newBufferWithLength:size
	                                     options:MTLResourceStorageModeShared];
	RTRLAB_ASSERT_MSG(m_Impl->buffer != nil, "MetalVertexBuffer: failed to allocate MTLBuffer");
}

MetalVertexBuffer::MetalVertexBuffer(const void *data, uint32_t size, BufferUsage usage)
	: m_Impl(std::make_unique<Impl>()), m_Usage(usage), m_AllocatedSize(size)
{
	id<MTLDevice> device = GetMTLDeviceFromGlobal();

	if (usage == BufferUsage::StaticDraw)
	{
		// Managed: CPU uploads once, then GPU owns. Fast for static geometry.
		m_Impl->buffer = [device newBufferWithBytes:data
		                                    length:size
		                                   options:MTLResourceStorageModeManaged];
	}
	else
	{
		// Shared: suitable for frequently updated buffers.
		m_Impl->buffer = [device newBufferWithBytes:data
		                                    length:size
		                                   options:MTLResourceStorageModeShared];
	}

	RTRLAB_ASSERT_MSG(m_Impl->buffer != nil, "MetalVertexBuffer: failed to allocate MTLBuffer");
}

MetalVertexBuffer::~MetalVertexBuffer() = default;

// --- IVertexBuffer ---

void MetalVertexBuffer::SetData(const void *data, uint32_t size, uint32_t offset)
{
	ERR_FAIL_COND_MSG_CAT(LogCategory::Graphics, data == nullptr, "MetalVertexBuffer::SetData received null data");
	RTRLAB_ASSERT_MSG(offset + size <= m_AllocatedSize, "MetalVertexBuffer::SetData: write exceeds allocated buffer size");

	uint8_t *dst = static_cast<uint8_t *>(m_Impl->buffer.contents);
	if (!dst)
	{
		LOG_WARN_CAT(LogCategory::Graphics, "MetalVertexBuffer::SetData: buffer has no CPU-accessible contents (managed static?)");
		return;
	}
	memcpy(dst + offset, data, size);

	if (m_Usage == BufferUsage::StaticDraw)
	{
		// Signal the managed buffer that the given range was modified.
		[m_Impl->buffer didModifyRange:NSMakeRange(offset, size)];
	}
}

// --- Metal-internal ---

void *MetalVertexBuffer::GetMTLBuffer() const
{
	return (__bridge void *)m_Impl->buffer;
}
