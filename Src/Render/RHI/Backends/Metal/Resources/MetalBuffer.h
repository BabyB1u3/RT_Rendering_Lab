#pragma once

/// @file MetalBuffer.h
/// @brief Backend-private Metal buffer wrapper.

#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/RHIResources.h"

class MetalBuffer final : public Buffer
{
public:
    MetalBuffer(id<MTLBuffer> buffer, const BufferDesc& desc) : m_Buffer([buffer retain]), m_Desc(desc) {}

    ~MetalBuffer() override
    {
        if (m_Buffer != nil)
        {
            [m_Buffer release];
            m_Buffer = nil;
        }
    }

    const BufferDesc& GetDesc() const override { return m_Desc; }
    id<MTLBuffer> GetMetalBuffer() const { return m_Buffer; }

private:
    id<MTLBuffer> m_Buffer = nil;
    BufferDesc m_Desc;
};

MetalBuffer& GetMetalBuffer(Buffer* buffer);
