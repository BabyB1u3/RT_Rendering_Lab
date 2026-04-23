#pragma once

/// @file MetalSampler.h
/// @brief Backend-private Metal sampler wrapper.

#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/RHIResources.h"

class MetalSampler final : public Sampler
{
public:
    MetalSampler(id<MTLSamplerState> sampler, const SamplerDesc& desc) : m_Sampler([sampler retain]), m_Desc(desc) {}

    ~MetalSampler() override
    {
        if (m_Sampler != nil)
        {
            [m_Sampler release];
            m_Sampler = nil;
        }
    }

    const SamplerDesc& GetDesc() const override { return m_Desc; }
    id<MTLSamplerState> GetMetalSampler() const { return m_Sampler; }

private:
    id<MTLSamplerState> m_Sampler = nil;
    SamplerDesc m_Desc;
};

const MetalSampler& GetMetalSampler(Sampler* sampler);
