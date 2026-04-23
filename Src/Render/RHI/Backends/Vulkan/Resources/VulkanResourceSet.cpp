#include "Render/RHI/Backends/Vulkan/Resources/VulkanResourceSet.h"

#include <algorithm>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanBuffer.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanSampler.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanTexture.h"

using namespace VulkanRHI;

VulkanResourceSet::VulkanResourceSet(VkDevice device,
                                     PipelineLayout* layout,
                                     uint32_t setIndex,
                                     VkDescriptorPool descriptorPool,
                                     VkDescriptorSet descriptorSet,
                                     uint32_t frameSlotCount)
    : m_Device(device),
      m_Layout(layout),
      m_SetIndex(setIndex),
      m_DescriptorPool(descriptorPool),
      m_DescriptorSet(descriptorSet),
      m_FrameConstantCaches(frameSlotCount)
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Vulkan ResourceSet creation requires a valid PipelineLayout.");
    if (const BindingInfo* constantBindingInfo =
            RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        constantBindingInfo != nullptr && constantBindingInfo->m_ByteSize > 0)
    {
        m_Constants.Resize(constantBindingInfo->m_ByteSize);
    }
}

VulkanResourceSet::~VulkanResourceSet()
{
    if (m_Device != VK_NULL_HANDLE && m_DescriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
}

void VulkanResourceSet::SetConstantDataRaw(uint32_t offset, const void* data, size_t size)
{
    if (size == 0)
        return;

    const BindingInfo& bindingInfo = ValidateConstantBindingExists();
    RTRLAB_ASSERT_MSG(offset + size <= bindingInfo.m_ByteSize,
                      "Vulkan ResourceSet constant write exceeds the declared UniformBuffer size.");
    m_Constants.SetRaw(offset, data, size);
    ++m_Version;
}

void VulkanResourceSet::SetBufferArray(uint32_t binding, std::span<const BufferBinding> bufferBindings)
{
    const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::StorageBuffer);
    ValidateBindingArrayCount(bindingInfo, bufferBindings.size(), "buffer");
    m_BufferBindings[binding] = std::vector<BufferBinding>(bufferBindings.begin(), bufferBindings.end());
    WriteBufferDescriptor(bindingInfo, m_BufferBindings[binding]);
    ++m_Version;
}

void VulkanResourceSet::SetTextureArray(uint32_t binding, std::span<const TextureBinding> textureBindings)
{
    const BindingInfo* bindingInfo =
        RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::SampledTexture);
    if (bindingInfo == nullptr)
        bindingInfo =
            RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::StorageTexture);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "Vulkan ResourceSet set {} has no texture binding {} in its PipelineLayout.",
                   m_SetIndex,
                   binding);
    ValidateBindingArrayCount(*bindingInfo, textureBindings.size(), "texture");

    std::vector<TextureBinding> resolvedBindings(textureBindings.begin(), textureBindings.end());
    ResolveAutoTextureViews(binding, resolvedBindings);

    m_TextureBindings[binding] = std::move(resolvedBindings);
    WriteTextureDescriptor(*bindingInfo, m_TextureBindings[binding]);
    ++m_Version;
}

void VulkanResourceSet::SetSamplerArray(uint32_t binding, std::span<const SamplerBinding> samplerBindings)
{
    const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::Sampler);
    ValidateBindingArrayCount(bindingInfo, samplerBindings.size(), "sampler");
    m_SamplerBindings[binding] = std::vector<SamplerBinding>(samplerBindings.begin(), samplerBindings.end());
    WriteSamplerDescriptor(bindingInfo, m_SamplerBindings[binding]);
    ++m_Version;
}

bool VulkanResourceSet::HasConstantBinding() const
{
    return RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer) !=
           nullptr;
}

bool VulkanResourceSet::NeedsConstantUploadForFrame(uint32_t frameSlot, uint64_t frameSerial) const
{
    RTRLAB_ASSERT_MSG(frameSlot < m_FrameConstantCaches.size(),
                      "Vulkan ResourceSet frame-slot index is out of range for constant uploads.");
    const FrameConstantCache& cache = m_FrameConstantCaches[frameSlot];
    return cache.m_Version != m_Version || cache.m_FrameSerial != frameSerial;
}

void VulkanResourceSet::WriteConstantDescriptorForFrame(
    uint32_t frameSlot, VkBuffer uploadBuffer, VkDeviceSize offset, VkDeviceSize size, uint64_t frameSerial)
{
    RTRLAB_ASSERT_MSG(frameSlot < m_FrameConstantCaches.size(),
                      "Vulkan ResourceSet frame-slot index is out of range for constant descriptor writes.");
    RTRLAB_ASSERT_MSG(uploadBuffer != VK_NULL_HANDLE,
                      "Vulkan ResourceSet constant descriptor writes require a valid frame upload buffer.");

    const BindingInfo& bindingInfo = RequireConstantBindingInfo();

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uploadBuffer;
    bufferInfo.offset = offset;
    bufferInfo.range = std::max<VkDeviceSize>(size, 1);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = bindingInfo.m_Binding;
    write.descriptorCount = 1;
    write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);

    FrameConstantCache& cache = m_FrameConstantCaches[frameSlot];
    cache.m_Version = m_Version;
    cache.m_FrameSerial = frameSerial;
    cache.m_Offset = offset;
    cache.m_Size = bufferInfo.range;
}

VkDescriptorType VulkanResourceSet::ToDescriptorType(ResourceKind resourceKind)
{
    switch (resourceKind)
    {
        case ResourceKind::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case ResourceKind::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case ResourceKind::SampledTexture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ResourceKind::StorageTexture:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case ResourceKind::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
    }

    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

const BindingInfo& VulkanResourceSet::RequireBindingInfo(uint32_t binding, ResourceKind kind) const
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Vulkan ResourceSet binding validation requires a valid PipelineLayout.");
    const BindingInfo* bindingInfo = RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, kind);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "Vulkan ResourceSet set {} has no binding {} of expected kind {} in its PipelineLayout.",
                   m_SetIndex,
                   binding,
                   static_cast<uint32_t>(kind));
    return *bindingInfo;
}

const BindingInfo& VulkanResourceSet::ValidateConstantBindingExists() const
{
    RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Vulkan ResourceSet constant validation requires a valid PipelineLayout.");
    const BindingInfo* bindingInfo =
        RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
    RTRLAB_ASSERTF(bindingInfo != nullptr,
                   "Vulkan ResourceSet set {} has no UniformBuffer binding in its PipelineLayout.",
                   m_SetIndex);
    return *bindingInfo;
}

void VulkanResourceSet::ValidateBindingArrayCount(const BindingInfo& bindingInfo,
                                                  size_t providedCount,
                                                  std::string_view resourceKind) const
{
    RTRLAB_ASSERTF(providedCount == bindingInfo.m_ArrayCount,
                   "Vulkan ResourceSet set {} binding {} expects exactly {} {} descriptor(s), but received {}.",
                   m_SetIndex,
                   bindingInfo.m_Binding,
                   bindingInfo.m_ArrayCount,
                   resourceKind,
                   providedCount);
}

const BindingInfo& VulkanResourceSet::RequireConstantBindingInfo() const
{
    const BindingInfo& bindingInfo = ValidateConstantBindingExists();
    RTRLAB_ASSERT_MSG(bindingInfo.m_ArrayCount <= 1,
                      "Vulkan ResourceSet constant uploads currently only support non-array UniformBuffer bindings.");
    return bindingInfo;
}

void VulkanResourceSet::WriteBufferDescriptor(const BindingInfo& bindingInfo,
                                              std::span<const BufferBinding> bufferBindings)
{
    std::vector<VkDescriptorBufferInfo> bufferInfos(bufferBindings.size());
    for (size_t index = 0; index < bufferBindings.size(); ++index)
    {
        const BufferBinding& bufferBinding = bufferBindings[index];
        RTRLAB_ASSERT_MSG(bufferBinding.m_Buffer != nullptr,
                          "Vulkan ResourceSet buffer descriptor writes require valid Buffers.");
        VulkanBuffer& vulkanBuffer = GetVulkanBuffer(bufferBinding.m_Buffer);

        VkDescriptorBufferInfo& bufferInfo = bufferInfos[index];
        bufferInfo.buffer = vulkanBuffer.GetVkBuffer();
        bufferInfo.offset = bufferBinding.m_Offset;
        bufferInfo.range = bufferBinding.m_Size == 0 ? VK_WHOLE_SIZE : bufferBinding.m_Size;
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = bindingInfo.m_Binding;
    write.descriptorCount = static_cast<uint32_t>(bufferInfos.size());
    write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
    write.pBufferInfo = bufferInfos.data();

    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanResourceSet::WriteTextureDescriptor(const BindingInfo& bindingInfo,
                                               std::span<const TextureBinding> textureBindings)
{
    std::vector<VkDescriptorImageInfo> imageInfos(textureBindings.size());
    for (size_t index = 0; index < textureBindings.size(); ++index)
    {
        const TextureBinding& textureBinding = textureBindings[index];
        RTRLAB_ASSERT_MSG(textureBinding.m_View != nullptr || textureBinding.m_Texture != nullptr,
                          "Vulkan ResourceSet texture descriptor writes require valid Textures or TextureViews.");

        VkDescriptorImageInfo& imageInfo = imageInfos[index];
        imageInfo.imageView = GetVkImageViewFromTextureView(textureBinding.m_View);
        imageInfo.imageLayout = bindingInfo.m_Kind == ResourceKind::StorageTexture
                                    ? VK_IMAGE_LAYOUT_GENERAL
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = bindingInfo.m_Binding;
    write.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
    write.pImageInfo = imageInfos.data();

    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanResourceSet::WriteSamplerDescriptor(const BindingInfo& bindingInfo,
                                               std::span<const SamplerBinding> samplerBindings)
{
    std::vector<VkDescriptorImageInfo> imageInfos(samplerBindings.size());
    for (size_t index = 0; index < samplerBindings.size(); ++index)
    {
        const SamplerBinding& samplerBinding = samplerBindings[index];
        RTRLAB_ASSERT_MSG(samplerBinding.m_Sampler != nullptr,
                          "Vulkan ResourceSet sampler descriptor writes require valid Samplers.");
        VulkanSampler& vulkanSampler = GetVulkanSampler(samplerBinding.m_Sampler);

        VkDescriptorImageInfo& imageInfo = imageInfos[index];
        imageInfo.sampler = vulkanSampler.GetVkSampler();
    }

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = bindingInfo.m_Binding;
    write.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
    write.pImageInfo = imageInfos.data();

    vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
}

void VulkanResourceSet::ResolveAutoTextureViews(uint32_t binding, std::vector<TextureBinding>& textureBindings)
{
    std::vector<Scope<TextureView>>& autoViews = m_AutoTextureViews[binding];
    autoViews.clear();
    autoViews.resize(textureBindings.size());

    for (size_t index = 0; index < textureBindings.size(); ++index)
    {
        TextureBinding& textureBinding = textureBindings[index];
        if (textureBinding.m_View == nullptr && textureBinding.m_Texture != nullptr)
        {
            autoViews[index] = CreateDefaultVulkanTextureView(m_Device, textureBinding.m_Texture);
            textureBinding.m_View = autoViews[index].get();
        }

        if (textureBinding.m_Texture == nullptr && textureBinding.m_View != nullptr)
            textureBinding.m_Texture = textureBinding.m_View->GetTexture();
    }
}
