#pragma once

/// @file MetalResourceSet.h
/// @brief Backend-private Metal resource-set wrapper with argument-buffer
/// encoding caches.

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/RHIPipeline.h"
#include "Render/RHI/RHIResources.h"

class MetalResourceSet final : public ResourceSet
{
public:
    MetalResourceSet(id<MTLDevice> device, PipelineLayout* layout, uint32_t setIndex);
    ~MetalResourceSet() override;

    PipelineLayout* GetLayout() const override { return m_Layout; }
    uint32_t GetSetIndex() const override { return m_SetIndex; }

    const ParameterBlockData& GetConstants() const override { return m_Constants; }
    void SetConstantDataRaw(uint32_t offset, const void* data, size_t size) override;
    void SetBufferArray(uint32_t binding, std::span<const BufferBinding> bufferBindings) override;
    void SetTextureArray(uint32_t binding, std::span<const TextureBinding> textureBindings) override;
    void SetSamplerArray(uint32_t binding, std::span<const SamplerBinding> samplerBindings) override;

    uint32_t GetVersion() const override { return m_Version; }
    id<MTLBuffer> GetEncodedArgumentBuffer(const MetalRHI::MetalStageBindingPlan& stagePlan,
                                           id<MTLArgumentEncoder> argumentEncoder);

private:
    struct StageArgumentBufferCache
    {
        ShaderStage m_Stage = ShaderStage::None;
        id<MTLBuffer> m_ArgumentBuffer = nil;
        uint32_t m_EncodedVersion = std::numeric_limits<uint32_t>::max();
    };

    const BindingInfo& RequireBindingInfo(uint32_t binding, ResourceKind kind) const;
    const BindingInfo& ValidateConstantBindingExists() const;
    void ValidateBindingArrayCount(const BindingInfo& bindingInfo,
                                   size_t providedCount,
                                   std::string_view resourceKind) const;
    void EnsureConstantBufferCapacity(size_t requiredSize);
    StageArgumentBufferCache& GetStageCache(ShaderStage stage);
    void EnsureArgumentBufferCapacity(StageArgumentBufferCache& cache, NSUInteger requiredLength);
    void EncodeStageArgumentBuffer(const MetalRHI::MetalStageBindingPlan& stagePlan,
                                   id<MTLArgumentEncoder> argumentEncoder);

    id<MTLDevice> m_Device = nil;
    PipelineLayout* m_Layout = nullptr;
    uint32_t m_SetIndex = 0;
    ParameterBlockData m_Constants;
    std::unordered_map<uint32_t, std::vector<BufferBinding>> m_BufferBindings;
    std::unordered_map<uint32_t, std::vector<TextureBinding>> m_TextureBindings;
    std::unordered_map<uint32_t, std::vector<SamplerBinding>> m_SamplerBindings;
    id<MTLBuffer> m_ConstantBuffer = nil;
    std::array<StageArgumentBufferCache, 3> m_StageCaches = {
        StageArgumentBufferCache{ShaderStage::Vertex, nil, std::numeric_limits<uint32_t>::max()},
        StageArgumentBufferCache{ShaderStage::Fragment, nil, std::numeric_limits<uint32_t>::max()},
        StageArgumentBufferCache{ShaderStage::Compute, nil, std::numeric_limits<uint32_t>::max()},
    };
    uint32_t m_Version = 0;
};

MetalResourceSet& GetMetalResourceSet(ResourceSet* resourceSet);
