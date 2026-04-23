#include "Render/RHI/Backends/Metal/Resources/MetalResourceSet.h"

#include <cstring>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Metal/Resources/MetalBuffer.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/Backends/Metal/Resources/MetalSampler.h"
#include "Render/RHI/Backends/Metal/Resources/MetalTexture.h"

using namespace MetalRHI;

MetalResourceSet::MetalResourceSet(id<MTLDevice> device, PipelineLayout* layout, uint32_t setIndex)
    : m_Device([device retain]), m_Layout(layout), m_SetIndex(setIndex)
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Metal ResourceSet creation requires a valid PipelineLayout.");
    const std::vector<const BindingInfo*> setBindings =
        RHIInternal::CollectBindingInfosForSet(m_Layout->GetDesc(), m_SetIndex);
    RTRLAB_ASSERTF(
        !setBindings.empty(), "Metal ResourceSet set {} does not exist in the provided PipelineLayout.", m_SetIndex);

    if (const BindingInfo* constantBindingInfo =
            RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        constantBindingInfo != nullptr && constantBindingInfo->m_ByteSize > 0)
    {
        m_Constants.Resize(constantBindingInfo->m_ByteSize);
    }
}

MetalResourceSet::~MetalResourceSet()
{
    for (StageArgumentBufferCache& cache : m_StageCaches)
    {
        if (cache.m_ArgumentBuffer != nil)
        {
            [cache.m_ArgumentBuffer release];
            cache.m_ArgumentBuffer = nil;
        }
    }

    if (m_ConstantBuffer != nil)
    {
        [m_ConstantBuffer release];
        m_ConstantBuffer = nil;
    }

    if (m_Device != nil)
    {
        [m_Device release];
        m_Device = nil;
    }
}

void MetalResourceSet::SetConstantDataRaw(uint32_t offset, const void* data, size_t size)
{
    if (size == 0)
        return;

    const BindingInfo& bindingInfo = ValidateConstantBindingExists();
    RTRLAB_ASSERT_MSG(offset + size <= bindingInfo.m_ByteSize,
                      "Metal ResourceSet constant write exceeds the declared UniformBuffer size.");
    m_Constants.SetRaw(offset, data, size);
    EnsureConstantBufferCapacity(m_Constants.GetSize());
    if (m_ConstantBuffer != nil && m_Constants.GetData() != nullptr)
        std::memcpy([m_ConstantBuffer contents], m_Constants.GetData(), m_Constants.GetSize());
    ++m_Version;
}

void MetalResourceSet::SetBufferArray(uint32_t binding, std::span<const BufferBinding> bufferBindings)
{
    const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::StorageBuffer);
    ValidateBindingArrayCount(bindingInfo, bufferBindings.size(), "buffer");
    m_BufferBindings[binding] = std::vector<BufferBinding>(bufferBindings.begin(), bufferBindings.end());
    ++m_Version;
}

void MetalResourceSet::SetTextureArray(uint32_t binding, std::span<const TextureBinding> textureBindings)
{
    const BindingInfo* bindingInfo =
        RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::SampledTexture);
    if (bindingInfo == nullptr)
        bindingInfo =
            RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::StorageTexture);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "Metal ResourceSet set {} has no texture binding {} in its PipelineLayout.",
                   m_SetIndex,
                   binding);
    ValidateBindingArrayCount(*bindingInfo, textureBindings.size(), "texture");
    m_TextureBindings[binding] = std::vector<TextureBinding>(textureBindings.begin(), textureBindings.end());
    ++m_Version;
}

void MetalResourceSet::SetSamplerArray(uint32_t binding, std::span<const SamplerBinding> samplerBindings)
{
    const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::Sampler);
    ValidateBindingArrayCount(bindingInfo, samplerBindings.size(), "sampler");
    m_SamplerBindings[binding] = std::vector<SamplerBinding>(samplerBindings.begin(), samplerBindings.end());
    ++m_Version;
}

id<MTLBuffer> MetalResourceSet::GetEncodedArgumentBuffer(const MetalStageBindingPlan& stagePlan,
                                                         id<MTLArgumentEncoder> argumentEncoder)
{
    RTRLAB_ASSERT_MSG(argumentEncoder != nil, "Metal argument-buffer encoding requires a valid encoder.");
    StageArgumentBufferCache& cache = GetStageCache(stagePlan.m_Stage);
    const NSUInteger requiredLength = std::max<NSUInteger>([argumentEncoder encodedLength], 1u);
    EnsureArgumentBufferCapacity(cache, requiredLength);
    if (cache.m_ArgumentBuffer == nil)
        return nil;

    if (cache.m_EncodedVersion == m_Version)
        return cache.m_ArgumentBuffer;

    [argumentEncoder setArgumentBuffer:cache.m_ArgumentBuffer offset:0];
    EncodeStageArgumentBuffer(stagePlan, argumentEncoder);
    cache.m_EncodedVersion = m_Version;
    return cache.m_ArgumentBuffer;
}

const BindingInfo& MetalResourceSet::RequireBindingInfo(uint32_t binding, ResourceKind kind) const
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Metal ResourceSet binding validation requires a valid PipelineLayout.");
    const BindingInfo* bindingInfo = RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, kind);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "Metal ResourceSet set {} has no binding {} of expected kind {} in its PipelineLayout.",
                   m_SetIndex,
                   binding,
                   static_cast<uint32_t>(kind));
    return *bindingInfo;
}

const BindingInfo& MetalResourceSet::ValidateConstantBindingExists() const
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Metal ResourceSet constant validation requires a valid PipelineLayout.");
    const BindingInfo* bindingInfo =
        RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "Metal ResourceSet set {} has no UniformBuffer binding in its PipelineLayout.",
                   m_SetIndex);
    RTRLAB_ASSERT_MSG(bindingInfo->m_ArrayCount <= 1,
                      "Metal ResourceSet constant writes currently only support non-array UniformBuffer bindings.");
    return *bindingInfo;
}

void MetalResourceSet::ValidateBindingArrayCount(const BindingInfo& bindingInfo,
                                                 size_t providedCount,
                                                 std::string_view resourceKind) const
{
    RTRLAB_ASSERTF(providedCount == bindingInfo.m_ArrayCount,
                   "Metal ResourceSet set {} binding {} expects exactly {} {} descriptor(s), but received {}.",
                   m_SetIndex,
                   bindingInfo.m_Binding,
                   bindingInfo.m_ArrayCount,
                   resourceKind,
                   providedCount);
}

void MetalResourceSet::EnsureConstantBufferCapacity(size_t requiredSize)
{
    if (requiredSize == 0)
        return;

    const NSUInteger requiredLength = static_cast<NSUInteger>(std::max<size_t>(requiredSize, 1));
    if (m_ConstantBuffer != nil && [m_ConstantBuffer length] >= requiredLength)
        return;

    id<MTLBuffer> newBuffer = [m_Device newBufferWithLength:requiredLength
                                                    options:ToMetalBufferResourceOptions(MemoryUsage::CpuToGpu)];
    RTRLAB_ASSERT_MSG(newBuffer != nil, "Failed to allocate the Metal ResourceSet constant buffer.");
    if (m_Constants.GetData() != nullptr)
        std::memcpy([newBuffer contents], m_Constants.GetData(), m_Constants.GetSize());

    if (m_ConstantBuffer != nil)
        [m_ConstantBuffer release];
    m_ConstantBuffer = newBuffer;
}

MetalResourceSet::StageArgumentBufferCache& MetalResourceSet::GetStageCache(ShaderStage stage)
{
    for (StageArgumentBufferCache& cache : m_StageCaches)
    {
        if (cache.m_Stage == stage)
            return cache;
    }

    RTRLAB_ASSERT_MSG(false, "Metal ResourceSet encountered an unsupported shader stage cache.");
    return m_StageCaches[0];
}

void MetalResourceSet::EnsureArgumentBufferCapacity(StageArgumentBufferCache& cache, NSUInteger requiredLength)
{
    if (cache.m_ArgumentBuffer != nil && [cache.m_ArgumentBuffer length] >= requiredLength)
        return;

    id<MTLBuffer> newBuffer = [m_Device newBufferWithLength:requiredLength
                                                    options:ToMetalBufferResourceOptions(MemoryUsage::CpuToGpu)];
    RTRLAB_ASSERT_MSG(newBuffer != nil, "Failed to allocate the Metal ResourceSet argument buffer.");
    if (cache.m_ArgumentBuffer != nil)
        [cache.m_ArgumentBuffer release];
    cache.m_ArgumentBuffer = newBuffer;
    cache.m_EncodedVersion = std::numeric_limits<uint32_t>::max();
}

void MetalResourceSet::EncodeStageArgumentBuffer(const MetalStageBindingPlan& stagePlan,
                                                 id<MTLArgumentEncoder> argumentEncoder)
{
    for (const MetalBindingPlanEntry& entry : stagePlan.m_Entries)
    {
        switch (entry.m_Kind)
        {
            case ResourceKind::UniformBuffer:
                [argumentEncoder setBuffer:m_ConstantBuffer offset:0 atIndex:entry.m_ArgumentIndex];
                break;
            case ResourceKind::StorageBuffer:
            {
                std::vector<id<MTLBuffer>> buffers(entry.m_ArrayCount, nil);
                std::vector<NSUInteger> offsets(entry.m_ArrayCount, 0);
                const auto it = m_BufferBindings.find(entry.m_Binding);
                const std::vector<BufferBinding>* bufferBindings = it != m_BufferBindings.end() ? &it->second : nullptr;
                if (bufferBindings != nullptr)
                {
                    RTRLAB_ASSERT_MSG(bufferBindings->size() == entry.m_ArrayCount,
                                      "Metal binding-plan array count drifted from the stored buffer bindings.");
                    for (uint32_t index = 0; index < entry.m_ArrayCount; ++index)
                    {
                        const BufferBinding& bufferBinding = (*bufferBindings)[index];
                        if (bufferBinding.m_Buffer == nullptr)
                            continue;

                        buffers[index] = GetMetalBuffer(bufferBinding.m_Buffer).GetMetalBuffer();
                        offsets[index] = static_cast<NSUInteger>(bufferBinding.m_Offset);
                    }
                }
                [argumentEncoder setBuffers:buffers.data()
                                    offsets:offsets.data()
                                  withRange:NSMakeRange(entry.m_ArgumentIndex, entry.m_ArrayCount)];
                break;
            }
            case ResourceKind::SampledTexture:
            case ResourceKind::StorageTexture:
            {
                std::vector<id<MTLTexture>> textures(entry.m_ArrayCount, nil);
                const auto it = m_TextureBindings.find(entry.m_Binding);
                const std::vector<TextureBinding>* textureBindings =
                    it != m_TextureBindings.end() ? &it->second : nullptr;
                if (textureBindings != nullptr)
                {
                    RTRLAB_ASSERT_MSG(textureBindings->size() == entry.m_ArrayCount,
                                      "Metal binding-plan array count drifted from the stored texture bindings.");
                    for (uint32_t index = 0; index < entry.m_ArrayCount; ++index)
                        textures[index] = ResolveMetalTextureForBinding((*textureBindings)[index]);
                }
                [argumentEncoder setTextures:textures.data()
                                   withRange:NSMakeRange(entry.m_ArgumentIndex, entry.m_ArrayCount)];
                break;
            }
            case ResourceKind::Sampler:
            {
                std::vector<id<MTLSamplerState>> samplers(entry.m_ArrayCount, nil);
                const auto it = m_SamplerBindings.find(entry.m_Binding);
                const std::vector<SamplerBinding>* samplerBindings =
                    it != m_SamplerBindings.end() ? &it->second : nullptr;
                if (samplerBindings != nullptr)
                {
                    RTRLAB_ASSERT_MSG(samplerBindings->size() == entry.m_ArrayCount,
                                      "Metal binding-plan array count drifted from the stored sampler bindings.");
                    for (uint32_t index = 0; index < entry.m_ArrayCount; ++index)
                    {
                        const SamplerBinding& samplerBinding = (*samplerBindings)[index];
                        if (samplerBinding.m_Sampler == nullptr)
                            continue;

                        samplers[index] = GetMetalSampler(samplerBinding.m_Sampler).GetMetalSampler();
                    }
                }
                [argumentEncoder setSamplerStates:samplers.data()
                                        withRange:NSMakeRange(entry.m_ArgumentIndex, entry.m_ArrayCount)];
                break;
            }
        }
    }
}

MetalResourceSet& GetMetalResourceSet(ResourceSet* resourceSet)
{
    auto* metalResourceSet = dynamic_cast<MetalResourceSet*>(resourceSet);
    RTRLAB_ASSERT_MSG(metalResourceSet != nullptr, "ResourceSet is not owned by the Metal backend.");
    return *metalResourceSet;
}
