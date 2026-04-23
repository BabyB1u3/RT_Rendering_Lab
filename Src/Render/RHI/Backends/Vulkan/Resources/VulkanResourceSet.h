#pragma once

/// @file VulkanResourceSet.h
/// @brief Backend-private Vulkan resource-set wrapper that owns a
/// descriptor pool, a descriptor set, and the per-frame constant-upload cache.

#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Util/Base.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHIPipeline.h"
#include "Render/RHI/RHIResources.h"

class VulkanResourceSet final : public ResourceSet
{
public:
    VulkanResourceSet(VkDevice device,
                      PipelineLayout* layout,
                      uint32_t setIndex,
                      VkDescriptorPool descriptorPool,
                      VkDescriptorSet descriptorSet,
                      uint32_t frameSlotCount);

    ~VulkanResourceSet() override;

    PipelineLayout* GetLayout() const override { return m_Layout; }
    uint32_t GetSetIndex() const override { return m_SetIndex; }

    const ParameterBlockData& GetConstants() const override { return m_Constants; }
    void SetConstantDataRaw(uint32_t offset, const void* data, size_t size) override;

    void SetBufferArray(uint32_t binding, std::span<const BufferBinding> bufferBindings) override;
    void SetTextureArray(uint32_t binding, std::span<const TextureBinding> textureBindings) override;
    void SetSamplerArray(uint32_t binding, std::span<const SamplerBinding> samplerBindings) override;

    uint32_t GetVersion() const override { return m_Version; }

    VkDescriptorSet GetVkDescriptorSet() const { return m_DescriptorSet; }
    bool HasConstantBinding() const;
    bool NeedsConstantUploadForFrame(uint32_t frameSlot, uint64_t frameSerial) const;
    void WriteConstantDescriptorForFrame(
        uint32_t frameSlot, VkBuffer uploadBuffer, VkDeviceSize offset, VkDeviceSize size, uint64_t frameSerial);

private:
    struct FrameConstantCache
    {
        uint32_t m_Version = std::numeric_limits<uint32_t>::max();
        uint64_t m_FrameSerial = 0;
        VkDeviceSize m_Offset = 0;
        VkDeviceSize m_Size = 0;
    };

    static VkDescriptorType ToDescriptorType(ResourceKind resourceKind);

    const BindingInfo& RequireBindingInfo(uint32_t binding, ResourceKind kind) const;
    const BindingInfo& ValidateConstantBindingExists() const;
    void ValidateBindingArrayCount(const BindingInfo& bindingInfo,
                                   size_t providedCount,
                                   std::string_view resourceKind) const;
    const BindingInfo& RequireConstantBindingInfo() const;

    void WriteBufferDescriptor(const BindingInfo& bindingInfo, std::span<const BufferBinding> bufferBindings);
    void WriteTextureDescriptor(const BindingInfo& bindingInfo, std::span<const TextureBinding> textureBindings);
    void WriteSamplerDescriptor(const BindingInfo& bindingInfo, std::span<const SamplerBinding> samplerBindings);
    void ResolveAutoTextureViews(uint32_t binding, std::vector<TextureBinding>& textureBindings);

    VkDevice m_Device = VK_NULL_HANDLE;
    PipelineLayout* m_Layout = nullptr;
    uint32_t m_SetIndex = 0;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    ParameterBlockData m_Constants;
    std::vector<FrameConstantCache> m_FrameConstantCaches;
    std::unordered_map<uint32_t, std::vector<BufferBinding>> m_BufferBindings;
    std::unordered_map<uint32_t, std::vector<TextureBinding>> m_TextureBindings;
    std::unordered_map<uint32_t, std::vector<Scope<TextureView>>> m_AutoTextureViews;
    std::unordered_map<uint32_t, std::vector<SamplerBinding>> m_SamplerBindings;
    uint32_t m_Version = 0;
};

inline VulkanResourceSet& GetVulkanResourceSet(ResourceSet* resourceSet)
{
    auto* vulkanResourceSet = dynamic_cast<VulkanResourceSet*>(resourceSet);
    RTRLAB_ASSERT_MSG(vulkanResourceSet != nullptr, "ResourceSet is not owned by the Vulkan backend.");
    return *vulkanResourceSet;
}
