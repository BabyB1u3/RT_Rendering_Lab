#include "MetalUniformBuffer.h"

#import <Metal/Metal.h>

#include <cstring>

#include "core/diagnostics/Assert.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/backends/metal/MetalGraphicsDevice.h"

struct MetalUniformBuffer::Impl
{
    id<MTLBuffer> buffer;
};

static id<MTLDevice> GetMTLUniformDeviceFromGlobal()
{
    auto *dev = static_cast<MetalGraphicsDevice *>(GetDevice().get());
    return (__bridge id<MTLDevice>)dev->GetMTLDevice();
}

MetalUniformBuffer::MetalUniformBuffer(uint32_t size)
    : m_Impl(std::make_unique<Impl>()), m_Size(size)
{
    id<MTLDevice> device = GetMTLUniformDeviceFromGlobal();
    m_Impl->buffer = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    RTRLAB_ASSERT_MSG(m_Impl->buffer != nil, "MetalUniformBuffer: failed to allocate MTLBuffer");
}

MetalUniformBuffer::~MetalUniformBuffer() = default;

void MetalUniformBuffer::SetData(const void *data, uint32_t size, uint32_t offset)
{
    RTRLAB_ASSERT_MSG(offset + size <= m_Size, "MetalUniformBuffer::SetData: write exceeds allocated buffer size");
    RTRLAB_ASSERT_MSG(data != nullptr || size == 0, "MetalUniformBuffer::SetData: non-zero upload requires data");

    auto *dst = static_cast<uint8_t *>(m_Impl->buffer.contents);
    RTRLAB_ASSERT_MSG(dst != nullptr, "MetalUniformBuffer::SetData: buffer has no CPU-accessible contents");
    if (size > 0)
        std::memcpy(dst + offset, data, size);
}

void *MetalUniformBuffer::GetMTLBuffer() const
{
    return (__bridge void *)m_Impl->buffer;
}
