#include "Render/RHI/Backends/Vulkan/VulkanDevice.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/VulkanConversions.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#if defined(GLAB_GLFW_X11_NATIVE)
#include <X11/Xlib.h>
#ifdef None
#undef None
#endif
#ifdef Always
#undef Always
#endif
#endif
#if defined(GLAB_GLFW_WAYLAND_NATIVE)
#include <wayland-client.h>
#endif
#endif

using namespace VulkanRHI;

class VulkanSwapchainTexture final : public Texture
{
public:
    VulkanSwapchainTexture(VkImage image, const TextureDesc& desc) : m_Image(image), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
    void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }
    TextureState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(TextureState state) { m_CurrentState = state; }

private:
    VkImage m_Image = VK_NULL_HANDLE;
    TextureDesc m_Desc;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureState m_CurrentState = TextureState::Undefined;
};

class VulkanSwapchainImageView final : public TextureView
{
public:
    VulkanSwapchainImageView(VkDevice device, Texture* texture, VkImageView imageView, const TextureViewDesc& desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanSwapchainImageView() override
    {
        if (m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    VkImageView GetVkImageView() const { return m_ImageView; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    Texture* m_Texture = nullptr;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    TextureViewDesc m_Desc;
};

class VulkanBuffer final : public Buffer
{
public:
    VulkanBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation, const BufferDesc& desc)
        : m_Allocator(allocator), m_Buffer(buffer), m_Allocation(allocation), m_Desc(desc)
    {
    }

    ~VulkanBuffer() override
    {
        if (m_Allocator != nullptr && m_Buffer != VK_NULL_HANDLE)
            vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
    }

    const BufferDesc& GetDesc() const override { return m_Desc; }
    VkBuffer GetVkBuffer() const { return m_Buffer; }
    VmaAllocation GetVmaAllocation() const { return m_Allocation; }
    BufferState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(BufferState state) { m_CurrentState = state; }

private:
    VmaAllocator m_Allocator = nullptr;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = nullptr;
    BufferDesc m_Desc;
    BufferState m_CurrentState = BufferState::Undefined;
};

class VulkanTexture final : public Texture
{
public:
    VulkanTexture(VmaAllocator allocator, VkImage image, VmaAllocation allocation, const TextureDesc& desc)
        : m_Allocator(allocator), m_Image(image), m_Allocation(allocation), m_Desc(desc)
    {
    }

    ~VulkanTexture() override
    {
        if (m_Allocator != nullptr && m_Image != VK_NULL_HANDLE)
            vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
    }

    const TextureDesc& GetDesc() const override { return m_Desc; }
    VkImage GetVkImage() const { return m_Image; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }
    void SetCurrentLayout(VkImageLayout layout) { m_CurrentLayout = layout; }
    TextureState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(TextureState state) { m_CurrentState = state; }

private:
    VmaAllocator m_Allocator = nullptr;
    VkImage m_Image = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = nullptr;
    TextureDesc m_Desc;
    VkImageLayout m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureState m_CurrentState = TextureState::Undefined;
};

class VulkanTextureView final : public TextureView
{
public:
    VulkanTextureView(VkDevice device, Texture* texture, VkImageView imageView, const TextureViewDesc& desc)
        : m_Device(device), m_Texture(texture), m_ImageView(imageView), m_Desc(desc)
    {
    }

    ~VulkanTextureView() override
    {
        if (m_Device != VK_NULL_HANDLE && m_ImageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_Device, m_ImageView, nullptr);
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    VkImageView GetVkImageView() const { return m_ImageView; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    Texture* m_Texture = nullptr;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    TextureViewDesc m_Desc;
};

class VulkanSampler final : public Sampler
{
public:
    VulkanSampler(VkDevice device, VkSampler sampler, const SamplerDesc& desc)
        : m_Device(device), m_Sampler(sampler), m_Desc(desc)
    {
    }

    ~VulkanSampler() override
    {
        if (m_Device != VK_NULL_HANDLE && m_Sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_Device, m_Sampler, nullptr);
    }

    const SamplerDesc& GetDesc() const override { return m_Desc; }
    VkSampler GetVkSampler() const { return m_Sampler; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    SamplerDesc m_Desc;
};

class VulkanShaderProgram final : public ShaderProgram
{
public:
    struct StageModule
    {
        ShaderStage m_Stage = ShaderStage::None;
        std::string m_EntryPoint;
        VkShaderModule m_Module = VK_NULL_HANDLE;
    };

    VulkanShaderProgram(VkDevice device, const CompiledShaderProgramDesc& desc, std::vector<StageModule>&& modules)
        : m_Device(device), m_Reflection(desc.m_Reflection), m_Modules(std::move(modules))
    {
    }

    ~VulkanShaderProgram() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        for (const StageModule& module : m_Modules)
        {
            if (module.m_Module != VK_NULL_HANDLE)
                vkDestroyShaderModule(m_Device, module.m_Module, nullptr);
        }
    }

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override
    {
        return RHIInternal::BuildPipelineLayoutDescFromReflection(m_Reflection);
    }

    const StageModule* FindStage(ShaderStage stage) const
    {
        const auto it = std::find_if(
            m_Modules.begin(), m_Modules.end(), [stage](const StageModule& module) { return module.m_Stage == stage; });
        return it != m_Modules.end() ? &(*it) : nullptr;
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    ShaderReflectionData m_Reflection;
    std::vector<StageModule> m_Modules;
};

class VulkanVertexInputLayout final : public VertexInputLayout
{
public:
    VulkanVertexInputLayout(const VertexInputLayoutDesc& desc,
                            std::vector<VkVertexInputBindingDescription>&& bindings,
                            std::vector<VkVertexInputAttributeDescription>&& attributes)
        : m_Desc(desc), m_Bindings(std::move(bindings)), m_Attributes(std::move(attributes))
    {
    }

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }
    const std::vector<VkVertexInputBindingDescription>& GetVkBindings() const { return m_Bindings; }
    const std::vector<VkVertexInputAttributeDescription>& GetVkAttributes() const { return m_Attributes; }

private:
    VertexInputLayoutDesc m_Desc;
    std::vector<VkVertexInputBindingDescription> m_Bindings;
    std::vector<VkVertexInputAttributeDescription> m_Attributes;
};

class VulkanPipelineLayout final : public PipelineLayout
{
public:
    VulkanPipelineLayout(VkDevice device,
                         const PipelineLayoutDesc& desc,
                         VkPipelineLayout pipelineLayout,
                         std::vector<VkDescriptorSetLayout>&& descriptorSetLayouts)
        : m_Device(device),
          m_Desc(desc),
          m_PipelineLayout(pipelineLayout),
          m_DescriptorSetLayouts(std::move(descriptorSetLayouts))
    {
    }

    ~VulkanPipelineLayout() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        if (m_PipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);

        for (VkDescriptorSetLayout descriptorSetLayout : m_DescriptorSetLayouts)
        {
            if (descriptorSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(m_Device, descriptorSetLayout, nullptr);
        }
    }

    const PipelineLayoutDesc& GetDesc() const override { return m_Desc; }
    VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }
    VkDescriptorSetLayout GetVkDescriptorSetLayout(uint32_t setIndex) const
    {
        RTRLAB_ASSERT_MSG(setIndex < m_DescriptorSetLayouts.size(),
                          "Vulkan descriptor-set layout index is out of range.");
        return m_DescriptorSetLayouts[setIndex];
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    PipelineLayoutDesc m_Desc;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
};

namespace
{
VkImageView GetVkImageViewFromTextureView(TextureView* textureView);
Scope<TextureView> CreateDefaultVulkanTextureView(VkDevice device, Texture* texture);
} // namespace

class VulkanResourceSet final : public ResourceSet
{
public:
    VulkanResourceSet(VkDevice device,
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
    }

    ~VulkanResourceSet() override
    {
        if (m_Device != VK_NULL_HANDLE && m_DescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
    }

    PipelineLayout* GetLayout() const override { return m_Layout; }
    uint32_t GetSetIndex() const override { return m_SetIndex; }

    const ParameterBlockData& GetConstants() const override { return m_Constants; }
    void SetConstantDataRaw(uint32_t offset, const void* data, size_t size) override
    {
        if (size == 0)
            return;

        ValidateConstantBindingExists();
        m_Constants.SetRaw(offset, data, size);
        ++m_Version;
    }

    void SetBuffer(uint32_t binding, const BufferBinding& bufferBinding) override
    {
        const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::StorageBuffer);
        m_BufferBindings[binding] = bufferBinding;
        WriteBufferDescriptor(bindingInfo, bufferBinding);
        ++m_Version;
    }

    void SetTexture(uint32_t binding, const TextureBinding& textureBinding) override
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
        TextureBinding resolvedBinding = textureBinding;
        if (resolvedBinding.m_View == nullptr && resolvedBinding.m_Texture != nullptr)
            resolvedBinding.m_View = ResolveAutoTextureView(binding, resolvedBinding.m_Texture);
        else
            m_AutoTextureViews.erase(binding);

        if (resolvedBinding.m_Texture == nullptr && resolvedBinding.m_View != nullptr)
            resolvedBinding.m_Texture = resolvedBinding.m_View->GetTexture();

        m_TextureBindings[binding] = resolvedBinding;
        WriteTextureDescriptor(*bindingInfo, resolvedBinding);
        ++m_Version;
    }

    void SetSampler(uint32_t binding, const SamplerBinding& samplerBinding) override
    {
        const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::Sampler);
        m_SamplerBindings[binding] = samplerBinding;
        WriteSamplerDescriptor(bindingInfo, samplerBinding);
        ++m_Version;
    }

    uint32_t GetVersion() const override { return m_Version; }

    VkDescriptorSet GetVkDescriptorSet() const { return m_DescriptorSet; }
    bool HasConstantBinding() const
    {
        return RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer) !=
               nullptr;
    }
    bool NeedsConstantUploadForFrame(uint32_t frameSlot, uint64_t frameSerial) const
    {
        RTRLAB_ASSERT_MSG(frameSlot < m_FrameConstantCaches.size(),
                          "Vulkan ResourceSet frame-slot index is out of range for constant uploads.");
        const FrameConstantCache& cache = m_FrameConstantCaches[frameSlot];
        return cache.m_Version != m_Version || cache.m_FrameSerial != frameSerial;
    }
    void WriteConstantDescriptorForFrame(
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

private:
    struct FrameConstantCache
    {
        uint32_t m_Version = std::numeric_limits<uint32_t>::max();
        uint64_t m_FrameSerial = 0;
        VkDeviceSize m_Offset = 0;
        VkDeviceSize m_Size = 0;
    };

    static VkDescriptorType ToDescriptorType(ResourceKind resourceKind)
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

    const BindingInfo& RequireBindingInfo(uint32_t binding, ResourceKind kind) const
    {
        RTRLAB_ASSERT_MSG(m_Layout != nullptr,
                          "Vulkan ResourceSet binding validation requires a valid PipelineLayout.");
        const BindingInfo* bindingInfo = RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, kind);
        RTRLAB_ASSERTF(bindingInfo != nullptr,
                       "Vulkan ResourceSet set {} has no binding {} of expected kind {} in its PipelineLayout.",
                       m_SetIndex,
                       binding,
                       static_cast<uint32_t>(kind));
        return *bindingInfo;
    }

    void ValidateConstantBindingExists() const
    {
        RTRLAB_ASSERT_MSG(m_Layout != nullptr,
                          "Vulkan ResourceSet constant validation requires a valid PipelineLayout.");
        const BindingInfo* bindingInfo =
            RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        RTRLAB_ASSERTF(bindingInfo != nullptr,
                       "Vulkan ResourceSet set {} has no UniformBuffer binding in its PipelineLayout.",
                       m_SetIndex);
    }

    const BindingInfo& RequireConstantBindingInfo() const
    {
        ValidateConstantBindingExists();
        const BindingInfo* bindingInfo =
            RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        RTRLAB_ASSERT_MSG(bindingInfo != nullptr, "Vulkan ResourceSet failed to resolve its UniformBuffer binding.");
        RTRLAB_ASSERT_MSG(bindingInfo->m_ArrayCount <= 1,
                          "Vulkan ResourceSet constant uploads currently only support non-array UniformBuffer "
                          "bindings.");
        return *bindingInfo;
    }

    void WriteBufferDescriptor(const BindingInfo& bindingInfo, const BufferBinding& bufferBinding)
    {
        RTRLAB_ASSERT_MSG(bindingInfo.m_ArrayCount <= 1,
                          "Vulkan ResourceSet descriptor writes currently only support non-array buffer bindings.");
        RTRLAB_ASSERT_MSG(bufferBinding.m_Buffer != nullptr,
                          "Vulkan ResourceSet buffer descriptor writes require a valid Buffer.");
        auto* vulkanBuffer = dynamic_cast<VulkanBuffer*>(bufferBinding.m_Buffer);
        RTRLAB_ASSERT_MSG(vulkanBuffer != nullptr, "Buffer is not owned by the Vulkan backend.");

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = vulkanBuffer->GetVkBuffer();
        bufferInfo.offset = bufferBinding.m_Offset;
        bufferInfo.range = bufferBinding.m_Size == 0 ? VK_WHOLE_SIZE : bufferBinding.m_Size;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSet;
        write.dstBinding = bindingInfo.m_Binding;
        write.descriptorCount = 1;
        write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
    }

    void WriteTextureDescriptor(const BindingInfo& bindingInfo, const TextureBinding& textureBinding)
    {
        RTRLAB_ASSERT_MSG(bindingInfo.m_ArrayCount <= 1,
                          "Vulkan ResourceSet descriptor writes currently only support non-array texture bindings.");
        RTRLAB_ASSERT_MSG(textureBinding.m_View != nullptr || textureBinding.m_Texture != nullptr,
                          "Vulkan ResourceSet texture descriptor writes require a valid Texture or TextureView.");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = textureBinding.m_View != nullptr ? GetVkImageViewFromTextureView(textureBinding.m_View)
                                                               : GetVkImageViewFromTextureView(ResolveAutoTextureView(
                                                                     bindingInfo.m_Binding, textureBinding.m_Texture));
        imageInfo.imageLayout = bindingInfo.m_Kind == ResourceKind::StorageTexture
                                    ? VK_IMAGE_LAYOUT_GENERAL
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSet;
        write.dstBinding = bindingInfo.m_Binding;
        write.descriptorCount = 1;
        write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
    }

    void WriteSamplerDescriptor(const BindingInfo& bindingInfo, const SamplerBinding& samplerBinding)
    {
        RTRLAB_ASSERT_MSG(bindingInfo.m_ArrayCount <= 1,
                          "Vulkan ResourceSet descriptor writes currently only support non-array sampler bindings.");
        RTRLAB_ASSERT_MSG(samplerBinding.m_Sampler != nullptr,
                          "Vulkan ResourceSet sampler descriptor writes require a valid Sampler.");
        auto* vulkanSampler = dynamic_cast<VulkanSampler*>(samplerBinding.m_Sampler);
        RTRLAB_ASSERT_MSG(vulkanSampler != nullptr, "Sampler is not owned by the Vulkan backend.");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = vulkanSampler->GetVkSampler();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSet;
        write.dstBinding = bindingInfo.m_Binding;
        write.descriptorCount = 1;
        write.descriptorType = ToDescriptorType(bindingInfo.m_Kind);
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, 1, &write, 0, nullptr);
    }

    TextureView* ResolveAutoTextureView(uint32_t binding, Texture* texture)
    {
        RTRLAB_ASSERT_MSG(texture != nullptr,
                          "Vulkan ResourceSet automatic texture-view resolution requires a valid Texture.");

        const auto autoViewIt = m_AutoTextureViews.find(binding);
        if (autoViewIt != m_AutoTextureViews.end() && autoViewIt->second != nullptr &&
            autoViewIt->second->GetTexture() == texture)
        {
            return autoViewIt->second.get();
        }

        Scope<TextureView> autoView = CreateDefaultVulkanTextureView(m_Device, texture);
        TextureView* autoViewPtr = autoView.get();
        m_AutoTextureViews[binding] = std::move(autoView);
        return autoViewPtr;
    }

    VkDevice m_Device = VK_NULL_HANDLE;
    PipelineLayout* m_Layout = nullptr;
    uint32_t m_SetIndex = 0;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    ParameterBlockData m_Constants;
    std::vector<FrameConstantCache> m_FrameConstantCaches;
    std::unordered_map<uint32_t, BufferBinding> m_BufferBindings;
    std::unordered_map<uint32_t, TextureBinding> m_TextureBindings;
    std::unordered_map<uint32_t, Scope<TextureView>> m_AutoTextureViews;
    std::unordered_map<uint32_t, SamplerBinding> m_SamplerBindings;
    uint32_t m_Version = 0;
};

class VulkanGraphicsPipeline final : public GraphicsPipeline
{
public:
    VulkanGraphicsPipeline(VkDevice device,
                           const GraphicsPipelineDesc& desc,
                           VulkanPipelineLayout* pipelineLayout,
                           VkPipeline pipeline)
        : m_Device(device), m_Desc(desc), m_PipelineLayout(pipelineLayout), m_Pipeline(pipeline)
    {
    }

    ~VulkanGraphicsPipeline() override
    {
        if (m_Device == VK_NULL_HANDLE)
            return;

        if (m_Pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
    }

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
    VkPipeline GetVkPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetVkPipelineLayout() const
    {
        return m_PipelineLayout != nullptr ? m_PipelineLayout->GetVkPipelineLayout() : VK_NULL_HANDLE;
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    GraphicsPipelineDesc m_Desc;
    VulkanPipelineLayout* m_PipelineLayout = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
};

namespace
{
const VulkanShaderProgram& GetVulkanShaderProgram(ShaderProgram* shaderProgram)
{
    auto* vulkanShaderProgram = dynamic_cast<VulkanShaderProgram*>(shaderProgram);
    RTRLAB_ASSERT_MSG(vulkanShaderProgram != nullptr, "GraphicsPipeline requires a Vulkan shader program.");
    return *vulkanShaderProgram;
}

const VulkanVertexInputLayout& GetVulkanVertexInputLayout(VertexInputLayout* vertexInputLayout)
{
    auto* vulkanVertexInputLayout = dynamic_cast<VulkanVertexInputLayout*>(vertexInputLayout);
    RTRLAB_ASSERT_MSG(vulkanVertexInputLayout != nullptr, "GraphicsPipeline requires a Vulkan vertex input layout.");
    return *vulkanVertexInputLayout;
}

VulkanPipelineLayout& GetVulkanPipelineLayout(PipelineLayout* pipelineLayout)
{
    auto* vulkanPipelineLayout = dynamic_cast<VulkanPipelineLayout*>(pipelineLayout);
    RTRLAB_ASSERT_MSG(vulkanPipelineLayout != nullptr, "GraphicsPipeline requires a Vulkan pipeline layout.");
    return *vulkanPipelineLayout;
}

VulkanResourceSet& GetVulkanResourceSet(ResourceSet* resourceSet)
{
    auto* vulkanResourceSet = dynamic_cast<VulkanResourceSet*>(resourceSet);
    RTRLAB_ASSERT_MSG(vulkanResourceSet != nullptr, "ResourceSet is not owned by the Vulkan backend.");
    return *vulkanResourceSet;
}

const VulkanGraphicsPipeline& GetVulkanGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
    auto* vulkanGraphicsPipeline = dynamic_cast<VulkanGraphicsPipeline*>(graphicsPipeline);
    RTRLAB_ASSERT_MSG(vulkanGraphicsPipeline != nullptr, "Graphics pipeline is not owned by the Vulkan backend.");
    return *vulkanGraphicsPipeline;
}

VulkanBuffer& GetVulkanBuffer(Buffer* buffer)
{
    auto* vulkanBuffer = dynamic_cast<VulkanBuffer*>(buffer);
    RTRLAB_ASSERT_MSG(vulkanBuffer != nullptr, "Buffer is not owned by the Vulkan backend.");
    return *vulkanBuffer;
}

VkBuffer GetVkBufferFromBuffer(Buffer* buffer)
{
    return GetVulkanBuffer(buffer).GetVkBuffer();
}

TextureAspect ResolveDefaultTextureViewAspect(const TextureDesc& textureDesc)
{
    if (IsDepthFormat(textureDesc.m_Format))
    {
        return HasStencilComponent(textureDesc.m_Format) ? (TextureAspect::Depth | TextureAspect::Stencil)
                                                         : TextureAspect::Depth;
    }

    return TextureAspect::Color;
}

TextureViewDesc BuildDefaultTextureViewDesc(const TextureDesc& textureDesc)
{
    TextureViewDesc viewDesc;
    viewDesc.m_Type = textureDesc.m_Type;
    viewDesc.m_Format = textureDesc.m_Format;
    viewDesc.m_Aspect = ResolveDefaultTextureViewAspect(textureDesc);
    viewDesc.m_BaseMipLevel = 0;
    viewDesc.m_MipLevelCount = std::max(textureDesc.m_MipLevels, 1u);
    viewDesc.m_BaseArrayLayer = 0;
    viewDesc.m_ArrayLayerCount = std::max(textureDesc.m_ArrayLayers, 1u);
    return viewDesc;
}

VkImage GetVkImageFromTexture(Texture* texture)
{
    if (auto* vulkanTexture = dynamic_cast<VulkanTexture*>(texture))
        return vulkanTexture->GetVkImage();
    if (auto* swapchainTexture = dynamic_cast<VulkanSwapchainTexture*>(texture))
        return swapchainTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return VK_NULL_HANDLE;
}

VkImageView GetVkImageViewFromTextureView(TextureView* textureView)
{
    if (auto* vulkanTextureView = dynamic_cast<VulkanTextureView*>(textureView))
        return vulkanTextureView->GetVkImageView();
    if (auto* swapchainImageView = dynamic_cast<VulkanSwapchainImageView*>(textureView))
        return swapchainImageView->GetVkImageView();

    RTRLAB_ASSERT_MSG(false, "TextureView is not owned by the Vulkan backend.");
    return VK_NULL_HANDLE;
}

Scope<TextureView> CreateDefaultVulkanTextureView(VkDevice device, Texture* texture)
{
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan default texture-view creation requires a valid Texture.");

    const TextureDesc& textureDesc = texture->GetDesc();
    const TextureViewDesc viewDesc = BuildDefaultTextureViewDesc(textureDesc);

    VkImageViewCreateInfo createInfo = MakeVkStruct<VkImageViewCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO>();
    createInfo.image = GetVkImageFromTexture(texture);
    createInfo.viewType = ToVkImageViewType(viewDesc.m_Type);
    createInfo.format = ToVkFormat(viewDesc.m_Format);
    createInfo.subresourceRange.aspectMask = ToVkImageAspect(viewDesc.m_Aspect, viewDesc.m_Format);
    createInfo.subresourceRange.baseMipLevel = viewDesc.m_BaseMipLevel;
    createInfo.subresourceRange.levelCount = viewDesc.m_MipLevelCount;
    createInfo.subresourceRange.baseArrayLayer = viewDesc.m_BaseArrayLayer;
    createInfo.subresourceRange.layerCount = viewDesc.m_ArrayLayerCount;

    VkImageView imageView = VK_NULL_HANDLE;
    CheckVk(vkCreateImageView(device, &createInfo, nullptr, &imageView), "vkCreateImageView(default texture view)");
    const std::string debugName = MakeTextureViewDebugName(*texture);
    SetVulkanDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), debugName.c_str());

    return CreateScope<VulkanTextureView>(device, texture, imageView, viewDesc);
}

std::vector<VkDescriptorSetLayout> CreateVkDescriptorSetLayouts(VkDevice device, const PipelineLayoutDesc& desc)
{
    uint32_t maxSetIndex = 0;
    bool hasBindings = false;
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        maxSetIndex = std::max(maxSetIndex, binding.m_SetIndex);
        hasBindings = true;
    }

    if (!hasBindings)
        return {};

    std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindingsPerSet(maxSetIndex + 1);
    for (const BindingInfo& binding : desc.m_Bindings)
    {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = binding.m_Binding;
        vkBinding.descriptorType = ToVkDescriptorType(binding.m_Kind);
        vkBinding.descriptorCount = std::max(binding.m_ArrayCount, 1u);
        vkBinding.stageFlags = ToVkShaderStageFlags(binding.m_StageMask);
        bindingsPerSet[binding.m_SetIndex].push_back(vkBinding);
    }

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(bindingsPerSet.size(), VK_NULL_HANDLE);
    for (size_t setIndex = 0; setIndex < bindingsPerSet.size(); ++setIndex)
    {
        VkDescriptorSetLayoutCreateInfo createInfo =
            MakeVkStruct<VkDescriptorSetLayoutCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO>();
        createInfo.bindingCount = static_cast<uint32_t>(bindingsPerSet[setIndex].size());
        createInfo.pBindings = bindingsPerSet[setIndex].empty() ? nullptr : bindingsPerSet[setIndex].data();
        CheckVk(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayouts[setIndex]),
                "vkCreateDescriptorSetLayout");
    }

    return descriptorSetLayouts;
}

VkDescriptorPool CreateVkDescriptorPoolForSet(VkDevice device, const PipelineLayoutDesc& desc, uint32_t setIndex)
{
    std::vector<VkDescriptorPoolSize> poolSizes;

    for (const BindingInfo* bindingInfo : RHIInternal::CollectBindingInfosForSet(desc, setIndex))
    {
        const VkDescriptorType descriptorType = ToVkDescriptorType(bindingInfo->m_Kind);
        const uint32_t descriptorCount = std::max(bindingInfo->m_ArrayCount, 1u);

        auto it = std::find_if(poolSizes.begin(),
                               poolSizes.end(),
                               [descriptorType](const VkDescriptorPoolSize& poolSize)
                               { return poolSize.type == descriptorType; });
        if (it == poolSizes.end())
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = descriptorType;
            poolSize.descriptorCount = descriptorCount;
            poolSizes.push_back(poolSize);
        }
        else
        {
            it->descriptorCount += descriptorCount;
        }
    }

    VkDescriptorPoolCreateInfo createInfo =
        MakeVkStruct<VkDescriptorPoolCreateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO>();
    createInfo.maxSets = 1;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.empty() ? nullptr : poolSizes.data();

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    CheckVk(vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool), "vkCreateDescriptorPool");
    return descriptorPool;
}

VkDescriptorSet AllocateVkDescriptorSet(VkDevice device,
                                        VkDescriptorPool descriptorPool,
                                        const VulkanPipelineLayout& pipelineLayout,
                                        uint32_t setIndex)
{
    const VkDescriptorSetLayout descriptorSetLayout = pipelineLayout.GetVkDescriptorSetLayout(setIndex);
    RTRLAB_ASSERT_MSG(descriptorSetLayout != VK_NULL_HANDLE,
                      "Vulkan ResourceSet allocation requires a valid VkDescriptorSetLayout.");

    VkDescriptorSetAllocateInfo allocateInfo =
        MakeVkStruct<VkDescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO>();
    allocateInfo.descriptorPool = descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    CheckVk(vkAllocateDescriptorSets(device, &allocateInfo, &descriptorSet), "vkAllocateDescriptorSets");
    return descriptorSet;
}

VkPipelineLayout CreateVkPipelineLayout(VkDevice device,
                                        const PipelineLayoutDesc& desc,
                                        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts)
{
    std::vector<VkPushConstantRange> pushConstantRanges;
    pushConstantRanges.reserve(desc.m_PushConstants.size());
    for (const PushConstantRangeDesc& pushConstant : desc.m_PushConstants)
    {
        VkPushConstantRange vkPushConstantRange{};
        vkPushConstantRange.stageFlags = ToVkShaderStageFlags(pushConstant.m_StageMask);
        vkPushConstantRange.offset = pushConstant.m_Offset;
        vkPushConstantRange.size = pushConstant.m_Size;
        pushConstantRanges.push_back(vkPushConstantRange);
    }

    VkPipelineLayoutCreateInfo createInfo =
        MakeVkStruct<VkPipelineLayoutCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO>();
    createInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    createInfo.pSetLayouts = descriptorSetLayouts.data();
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    createInfo.pPushConstantRanges = pushConstantRanges.data();

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    CheckVk(vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
    return pipelineLayout;
}

VkImage GetVkImageFromOwnedTexture(Texture* texture)
{
    if (auto* ownedTexture = dynamic_cast<VulkanTexture*>(texture))
        return ownedTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false,
                      "Vulkan CreateTextureView only accepts device-created textures. "
                      "Swapchain images expose views via Swapchain::GetImageView().");
    return VK_NULL_HANDLE;
}

std::vector<const char*> GetPlatformInstanceExtensionCandidates()
{
    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

#if defined(_WIN32)
    extensions.push_back("VK_KHR_win32_surface");
#elif defined(__APPLE__)
    extensions.push_back("VK_EXT_metal_surface");
#elif defined(__linux__)
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    extensions.push_back("VK_KHR_xlib_surface");
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    extensions.push_back("VK_KHR_wayland_surface");
#endif
#endif

    return extensions;
}

std::vector<const char*> GetSupportedInstanceExtensions()
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
            "vkEnumerateInstanceExtensionProperties(count)");

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    CheckVk(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data()),
            "vkEnumerateInstanceExtensionProperties(list)");

    std::vector<const char*> enabledExtensions;
    for (const char* candidate : GetPlatformInstanceExtensionCandidates())
    {
        const auto it = std::find_if(availableExtensions.begin(),
                                     availableExtensions.end(),
                                     [candidate](const VkExtensionProperties& extension)
                                     { return std::strcmp(extension.extensionName, candidate) == 0; });
        if (it != availableExtensions.end())
            enabledExtensions.push_back(candidate);
    }

    const auto hasEnabledExtension = [&enabledExtensions](const char* extensionName)
    {
        return std::find_if(enabledExtensions.begin(),
                            enabledExtensions.end(),
                            [extensionName](const char* enabledExtension)
                            { return std::strcmp(enabledExtension, extensionName) == 0; }) != enabledExtensions.end();
    };

    const bool hasSurfaceExtension = hasEnabledExtension(VK_KHR_SURFACE_EXTENSION_NAME);
    RTRLAB_ASSERT_MSG(hasSurfaceExtension, "Vulkan instance creation requires VK_KHR_surface support.");
    return enabledExtensions;
}

VkSurfaceKHR CreateSurface(VkInstance instance, const NativeWindowHandle& nativeWindowHandle)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    switch (nativeWindowHandle.m_System)
    {
        case NativeWindowSystem::Win32:
        {
#if defined(_WIN32)
            VkWin32SurfaceCreateInfoKHR createInfo =
                MakeVkStruct<VkWin32SurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR>();
            createInfo.hinstance = GetModuleHandleW(nullptr);
            createInfo.hwnd = reinterpret_cast<HWND>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWin32SurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Win32 Vulkan surface creation is unavailable on this platform.");
#endif
            break;
        }
        case NativeWindowSystem::Cocoa:
            RTRLAB_ASSERT_MSG(false, "Cocoa Vulkan surface creation is not compiled in this backend build.");
            break;
        case NativeWindowSystem::Xlib:
        {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
            VkXlibSurfaceCreateInfoKHR createInfo =
                MakeVkStruct<VkXlibSurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR>();
            createInfo.dpy = static_cast<Display*>(nativeWindowHandle.m_Display);
            createInfo.window = static_cast<::Window>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateXlibSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Xlib Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
        case NativeWindowSystem::Xcb:
            RTRLAB_ASSERT_MSG(false, "XCB native window handles are not currently produced by the platform layer.");
            break;
        case NativeWindowSystem::Wayland:
        {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
            VkWaylandSurfaceCreateInfoKHR createInfo =
                MakeVkStruct<VkWaylandSurfaceCreateInfoKHR, VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR>();
            createInfo.display = static_cast<wl_display*>(nativeWindowHandle.m_Display);
            createInfo.surface = reinterpret_cast<wl_surface*>(nativeWindowHandle.m_Window);
            CheckVk(vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface), "vkCreateWaylandSurfaceKHR");
#else
            RTRLAB_ASSERT_MSG(false, "Wayland Vulkan surface creation is unavailable in this build.");
#endif
            break;
        }
    }

    return surface;
}

struct QueueFamilySelection
{
    uint32_t m_GraphicsFamily = std::numeric_limits<uint32_t>::max();
    uint32_t m_PresentFamily = std::numeric_limits<uint32_t>::max();

    bool IsComplete() const
    {
        return m_GraphicsFamily != std::numeric_limits<uint32_t>::max() &&
               m_PresentFamily != std::numeric_limits<uint32_t>::max();
    }
};

QueueFamilySelection FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    QueueFamilySelection selection{};

    for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        const VkQueueFamilyProperties& properties = familyProperties[familyIndex];
        if ((properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            selection.m_GraphicsFamily = familyIndex;

        VkBool32 presentSupported = VK_FALSE;
        CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &presentSupported),
                "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (presentSupported == VK_TRUE)
            selection.m_PresentFamily = familyIndex;

        if (selection.IsComplete())
            break;
    }

    return selection;
}

uint32_t FindGraphicsQueueFamily(VkPhysicalDevice physicalDevice)
{
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProperties.data());

    for (uint32_t familyIndex = 0; familyIndex < familyCount; ++familyIndex)
    {
        if ((familyProperties[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            return familyIndex;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan graphics queue family.");
    return std::numeric_limits<uint32_t>::max();
}

bool SupportsPresentOnQueueFamily(VkPhysicalDevice physicalDevice, uint32_t queueFamily, VkSurfaceKHR surface)
{
    VkBool32 presentSupported = VK_FALSE;
    CheckVk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &presentSupported),
            "vkGetPhysicalDeviceSurfaceSupportKHR");
    return presentSupported == VK_TRUE;
}

bool SupportsRequiredDeviceExtensions(VkPhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr),
            "vkEnumerateDeviceExtensionProperties(count)");

    std::vector<VkExtensionProperties> extensions(extensionCount);
    CheckVk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data()),
            "vkEnumerateDeviceExtensionProperties(list)");

    for (const VkExtensionProperties& extension : extensions)
    {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            return true;
    }

    return false;
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, QueueFamilySelection& selection)
{
    uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        if (!SupportsRequiredDeviceExtensions(device))
            continue;

        QueueFamilySelection candidateSelection = FindQueueFamilies(device, surface);
        if (!candidateSelection.IsComplete())
            continue;

        selection = candidateSelection;
        return device;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics, present, and swapchain support.");
    return VK_NULL_HANDLE;
}

VkPhysicalDevice PickPhysicalDevice(VkInstance instance, uint32_t& graphicsQueueFamily)
{
    uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    RTRLAB_ASSERT_MSG(deviceCount > 0, "No Vulkan physical devices are available.");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        if (!SupportsRequiredDeviceExtensions(device))
            continue;

        graphicsQueueFamily = FindGraphicsQueueFamily(device);
        if (graphicsQueueFamily == std::numeric_limits<uint32_t>::max())
            continue;

        return device;
    }

    RTRLAB_ASSERT_MSG(false, "Failed to find a Vulkan physical device with graphics and swapchain support.");
    return VK_NULL_HANDLE;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, Format preferredFormat)
{
    uint32_t formatCount = 0;
    CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr),
            "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    RTRLAB_ASSERT_MSG(formatCount > 0, "Vulkan surface reports no supported formats.");

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");

    const VkFormat preferredVkFormat = ToVkFormat(preferredFormat);
    for (const VkSurfaceFormatKHR& surfaceFormat : formats)
    {
        if (surfaceFormat.format == preferredVkFormat)
            return surfaceFormat;
    }

    return formats.front();
}

VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, bool vsync)
{
    uint32_t modeCount = 0;
    CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr),
            "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    RTRLAB_ASSERT_MSG(modeCount > 0, "Vulkan surface reports no present modes.");

    std::vector<VkPresentModeKHR> presentModes(modeCount);
    CheckVk(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, presentModes.data()),
            "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");

    if (vsync)
        return VK_PRESENT_MODE_FIFO_KHR;

    for (VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
    }

    for (VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return presentMode;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D
ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t requestedWidth, uint32_t requestedHeight)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    VkExtent2D extent{};
    extent.width = std::clamp(requestedWidth, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(requestedHeight, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

void TransitionImageLayout(VkCommandBuffer commandBuffer,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           VkPipelineStageFlags srcStageMask,
                           VkPipelineStageFlags dstStageMask,
                           VkAccessFlags srcAccessMask,
                           VkAccessFlags dstAccessMask,
                           VkImageAspectFlags aspectMask,
                           uint32_t mipLevelCount,
                           uint32_t arrayLayerCount)
{
    VkImageMemoryBarrier barrier = MakeVkStruct<VkImageMemoryBarrier, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER>();
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = arrayLayerCount;

    vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkPipelineStageFlags ToVkPipelineStageMask(ShaderStage stageMask)
{
    VkPipelineStageFlags result = 0;
    if ((stageMask & ShaderStage::Vertex) != ShaderStage::None)
        result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if ((stageMask & ShaderStage::Fragment) != ShaderStage::None)
        result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if ((stageMask & ShaderStage::Compute) != ShaderStage::None)
        result |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    return result;
}

TextureAspect GetFullTextureAspect(const TextureDesc& desc)
{
    if (IsDepthFormat(desc.m_Format))
    {
        return HasStencilComponent(desc.m_Format) ? (TextureAspect::Depth | TextureAspect::Stencil)
                                                  : TextureAspect::Depth;
    }

    return TextureAspect::Color;
}

VkImageLayout ToVkImageLayout(TextureState state, const TextureDesc& desc)
{
    switch (state)
    {
        case TextureState::Undefined:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        case TextureState::RenderTarget:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case TextureState::DepthStencil:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case TextureState::ShaderRead:
            return IsDepthFormat(desc.m_Format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case TextureState::ShaderReadWrite:
            return VK_IMAGE_LAYOUT_GENERAL;
        case TextureState::CopySource:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case TextureState::CopyDest:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case TextureState::Present:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkPipelineStageFlags TextureStateToPipelineStageMask(TextureState state, ShaderStage shaderStages, bool isDestination)
{
    switch (state)
    {
        case TextureState::RenderTarget:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case TextureState::DepthStencil:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case TextureState::ShaderRead:
        case TextureState::ShaderReadWrite:
        {
            const VkPipelineStageFlags shaderStageMask =
                ToVkPipelineStageMask(shaderStages == ShaderStage::None ? ShaderStage::All : shaderStages);
            return shaderStageMask != 0 ? shaderStageMask : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        case TextureState::CopySource:
        case TextureState::CopyDest:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case TextureState::Present:
            return isDestination ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case TextureState::Undefined:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags TextureStateToAccessMask(TextureState state)
{
    switch (state)
    {
        case TextureState::RenderTarget:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case TextureState::DepthStencil:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case TextureState::ShaderRead:
            return VK_ACCESS_SHADER_READ_BIT;
        case TextureState::ShaderReadWrite:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case TextureState::CopySource:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case TextureState::CopyDest:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case TextureState::Present:
        case TextureState::Undefined:
        default:
            return 0;
    }
}

VkPipelineStageFlags BufferStateToPipelineStageMask(BufferState state, ShaderStage shaderStages)
{
    switch (state)
    {
        case BufferState::VertexIndex:
            return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case BufferState::UniformRead:
        case BufferState::StorageRead:
        case BufferState::StorageReadWrite:
        {
            const VkPipelineStageFlags shaderStageMask =
                ToVkPipelineStageMask(shaderStages == ShaderStage::None ? ShaderStage::All : shaderStages);
            return shaderStageMask != 0 ? shaderStageMask : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        case BufferState::CopySource:
        case BufferState::CopyDest:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case BufferState::IndirectArgument:
            return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        case BufferState::Undefined:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags BufferStateToAccessMask(BufferState state)
{
    switch (state)
    {
        case BufferState::VertexIndex:
            return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
        case BufferState::UniformRead:
            return VK_ACCESS_UNIFORM_READ_BIT;
        case BufferState::StorageRead:
            return VK_ACCESS_SHADER_READ_BIT;
        case BufferState::StorageReadWrite:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case BufferState::CopySource:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case BufferState::CopyDest:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case BufferState::IndirectArgument:
            return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        case BufferState::Undefined:
        default:
            return 0;
    }
}

VulkanTexture* TryGetVulkanTexture(Texture* texture)
{
    return dynamic_cast<VulkanTexture*>(texture);
}

VulkanSwapchainTexture* TryGetVulkanSwapchainTexture(Texture* texture)
{
    return dynamic_cast<VulkanSwapchainTexture*>(texture);
}

VkImage GetVkImageForBarrier(Texture* texture)
{
    if (auto* ownedTexture = TryGetVulkanTexture(texture))
        return ownedTexture->GetVkImage();
    if (auto* swapchainTexture = TryGetVulkanSwapchainTexture(texture))
        return swapchainTexture->GetVkImage();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return VK_NULL_HANDLE;
}

VkImageLayout GetTrackedImageLayout(Texture* texture)
{
    if (auto* ownedTexture = TryGetVulkanTexture(texture))
        return ownedTexture->GetCurrentLayout();
    if (auto* swapchainTexture = TryGetVulkanSwapchainTexture(texture))
        return swapchainTexture->GetCurrentLayout();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

TextureState GetTrackedTextureState(Texture* texture)
{
    if (auto* ownedTexture = TryGetVulkanTexture(texture))
        return ownedTexture->GetCurrentState();
    if (auto* swapchainTexture = TryGetVulkanSwapchainTexture(texture))
        return swapchainTexture->GetCurrentState();

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
    return TextureState::Undefined;
}

void SetTrackedTextureState(Texture* texture, TextureState state, VkImageLayout layout)
{
    if (auto* ownedTexture = TryGetVulkanTexture(texture))
    {
        ownedTexture->SetCurrentState(state);
        ownedTexture->SetCurrentLayout(layout);
        return;
    }

    if (auto* swapchainTexture = TryGetVulkanSwapchainTexture(texture))
    {
        swapchainTexture->SetCurrentState(state);
        swapchainTexture->SetCurrentLayout(layout);
        return;
    }

    RTRLAB_ASSERT_MSG(false, "Texture is not owned by the Vulkan backend.");
}
} // namespace

VulkanCommandList::~VulkanCommandList()
{
    Shutdown();
}

void VulkanCommandList::Initialize(VulkanDevice* ownerDevice, VkDevice device, VkCommandPool commandPool)
{
    Shutdown();

    m_OwnerDevice = ownerDevice;
    m_Device = device;
    m_CommandPool = commandPool;

    VkCommandBufferAllocateInfo allocateInfo =
        MakeVkStruct<VkCommandBufferAllocateInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO>();
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    CheckVk(vkAllocateCommandBuffers(device, &allocateInfo, &m_CommandBuffer), "vkAllocateCommandBuffers");
}

void VulkanCommandList::Shutdown()
{
    // Idempotent: VulkanDevice tears the command list down explicitly before member destruction.
    if (m_Device != VK_NULL_HANDLE && m_CommandBuffer != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
    }

    m_CommandBuffer = VK_NULL_HANDLE;
    m_CommandPool = VK_NULL_HANDLE;
    m_Device = VK_NULL_HANDLE;
    m_OwnerDevice = nullptr;
}

void VulkanCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(renderingInfo.m_ColorAttachments.size() == 1,
                      "Early Vulkan bring-up currently supports exactly one color attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_DepthAttachment.m_View == nullptr,
                      "Early Vulkan bring-up does not support depth attachments yet.");

    const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<VulkanSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Vulkan BeginRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<VulkanSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan BeginRendering currently expects a swapchain texture.");
    RTRLAB_ASSERT_MSG(texture->GetCurrentState() == TextureState::RenderTarget,
                      "Vulkan BeginRendering requires the color attachment to be transitioned to RenderTarget "
                      "before BeginRendering.");
    RTRLAB_ASSERT_MSG(texture->GetCurrentLayout() == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                      "Vulkan BeginRendering requires the color attachment to already be in "
                      "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.");

    const VkClearValue clearValue = {{
        colorAttachment.m_ClearValue.m_R,
        colorAttachment.m_ClearValue.m_G,
        colorAttachment.m_ClearValue.m_B,
        colorAttachment.m_ClearValue.m_A,
    }};

    VkRenderingAttachmentInfo colorAttachmentInfo =
        MakeVkStruct<VkRenderingAttachmentInfo, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO>();
    colorAttachmentInfo.imageView = imageView->GetVkImageView();
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = colorAttachment.m_LoadOp == LoadOp::Clear      ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                 : colorAttachment.m_LoadOp == LoadOp::DontCare ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
                                                                                : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachmentInfo.storeOp = colorAttachment.m_StoreOp == StoreOp::DontCare ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                                                                 : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = clearValue;

    VkRenderingInfo vkRenderingInfo = MakeVkStruct<VkRenderingInfo, VK_STRUCTURE_TYPE_RENDERING_INFO>();
    vkRenderingInfo.renderArea.offset = {renderingInfo.m_RenderArea.m_X, renderingInfo.m_RenderArea.m_Y};
    vkRenderingInfo.renderArea.extent = {renderingInfo.m_RenderArea.m_Width, renderingInfo.m_RenderArea.m_Height};
    vkRenderingInfo.layerCount = 1;
    vkRenderingInfo.colorAttachmentCount = 1;
    vkRenderingInfo.pColorAttachments = &colorAttachmentInfo;

    vkCmdBeginRendering(m_CommandBuffer, &vkRenderingInfo);
    SetViewport(static_cast<float>(renderingInfo.m_RenderArea.m_X),
                static_cast<float>(renderingInfo.m_RenderArea.m_Y),
                static_cast<float>(renderingInfo.m_RenderArea.m_Width),
                static_cast<float>(renderingInfo.m_RenderArea.m_Height),
                0.0f,
                1.0f);
    SetScissor(renderingInfo.m_RenderArea.m_X,
               renderingInfo.m_RenderArea.m_Y,
               renderingInfo.m_RenderArea.m_Width,
               renderingInfo.m_RenderArea.m_Height);
}

void VulkanCommandList::EndRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan EndRendering requires an active rendering scope.");

    const ColorAttachmentInfo& colorAttachment = m_RenderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<VulkanSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Vulkan EndRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<VulkanSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan EndRendering currently expects a swapchain texture.");

    vkCmdEndRendering(m_CommandBuffer);
    RTRLAB_ASSERT_MSG(texture->GetCurrentState() == TextureState::RenderTarget,
                      "Vulkan EndRendering expects the render pass attachment to remain in RenderTarget state "
                      "until an explicit barrier transitions it elsewhere.");

    ShellCommandListBase::EndRendering();
}

void VulkanCommandList::BindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    ShellCommandListBase::BindGraphicsPipeline(pipeline);

    if (pipeline == nullptr)
        return;

    const VulkanGraphicsPipeline& vulkanPipeline = GetVulkanGraphicsPipeline(pipeline);
    vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline.GetVkPipeline());
}

void VulkanCommandList::BindComputePipeline(ComputePipeline*)
{
    RTRLAB_ASSERT_MSG(false,
                      "Vulkan compute pipelines are not implemented yet. This backend no longer falls back to the "
                      "shell compute path.");
}

void VulkanCommandList::BindResourceSet(uint32_t setIndex, ResourceSet* resourceSet)
{
    ShellCommandListBase::BindResourceSet(setIndex, resourceSet);

    if (resourceSet == nullptr)
        return;

    RTRLAB_ASSERTF(resourceSet->GetSetIndex() == setIndex,
                   "Vulkan BindResourceSet expected resource set {} but received set {}.",
                   setIndex,
                   resourceSet->GetSetIndex());

    VulkanResourceSet& vulkanResourceSet = GetVulkanResourceSet(resourceSet);
    PipelineLayout* layout = resourceSet->GetLayout();
    RTRLAB_ASSERT_MSG(layout != nullptr, "Vulkan BindResourceSet requires a valid PipelineLayout.");

    VulkanPipelineLayout& vulkanPipelineLayout = GetVulkanPipelineLayout(layout);
    const VkDescriptorSet descriptorSet = vulkanResourceSet.GetVkDescriptorSet();
    RTRLAB_ASSERT_MSG(descriptorSet != VK_NULL_HANDLE, "Vulkan BindResourceSet requires a valid VkDescriptorSet.");

    const VkPipelineLayout pipelineLayoutHandle = vulkanPipelineLayout.GetVkPipelineLayout();
    RTRLAB_ASSERT_MSG(pipelineLayoutHandle != VK_NULL_HANDLE,
                      "Vulkan BindResourceSet requires a valid VkPipelineLayout.");

    RTRLAB_ASSERT_MSG(m_OwnerDevice != nullptr, "Vulkan BindResourceSet requires an initialized owner device.");
    m_OwnerDevice->PrepareResourceSetForBinding(resourceSet);

    vkCmdBindDescriptorSets(m_CommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayoutHandle,
                            setIndex,
                            1,
                            &descriptorSet,
                            0,
                            nullptr);
}

void VulkanCommandList::PushConstants(ShaderStage, uint32_t, uint32_t, const void*)
{
    RTRLAB_ASSERT_MSG(false,
                      "Vulkan push constants are not implemented yet. This backend no longer records shell-only "
                      "push-constant state.");
}

void VulkanCommandList::BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets)
{
    ShellCommandListBase::BindMesh(meshBinding, vertexOffsets);

    if (!meshBinding.m_VertexBuffers.empty())
    {
        BindVertexBuffers(0,
                          meshBinding.m_VertexBuffers.data(),
                          static_cast<uint32_t>(meshBinding.m_VertexBuffers.size()),
                          vertexOffsets);
    }

    m_IndexBuffer = meshBinding.m_IndexBuffer;
    m_IndexOffset = 0;
    m_IndexType = meshBinding.m_IndexType;
    if (meshBinding.m_IndexBuffer != nullptr)
        BindIndexBuffer(meshBinding.m_IndexBuffer, 0, meshBinding.m_IndexType);
}

void VulkanCommandList::BindVertexBuffers(uint32_t firstSlot,
                                          Buffer* const* buffers,
                                          uint32_t count,
                                          const uint64_t* offsets)
{
    ShellCommandListBase::BindVertexBuffers(firstSlot, buffers, count, offsets);

    if (count == 0)
        return;

    std::vector<VkBuffer> vkBuffers(count, VK_NULL_HANDLE);
    std::vector<VkDeviceSize> vkOffsets(count, 0);
    for (uint32_t index = 0; index < count; ++index)
    {
        RTRLAB_ASSERT_MSG(buffers[index] != nullptr, "Vulkan BindVertexBuffers requires non-null buffers.");
        vkBuffers[index] = GetVkBufferFromBuffer(buffers[index]);
        vkOffsets[index] = offsets != nullptr ? offsets[index] : 0;
    }

    vkCmdBindVertexBuffers(m_CommandBuffer, firstSlot, count, vkBuffers.data(), vkOffsets.data());
}

void VulkanCommandList::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
{
    ShellCommandListBase::BindIndexBuffer(buffer, offset, indexType);

    if (buffer == nullptr)
        return;

    vkCmdBindIndexBuffer(m_CommandBuffer, GetVkBufferFromBuffer(buffer), offset, ToVkIndexType(indexType));
}

void VulkanCommandList::SetViewport(float x, float y, float w, float h, float zmin, float zmax)
{
    ShellCommandListBase::SetViewport(x, y, w, h, zmin, zmax);

    VkViewport viewport{};
    viewport.x = x;
    // Normalize Vulkan's framebuffer-space Y direction to the project's
    // public render policy by flipping the viewport in backend space instead
    // of forcing per-shader or per-projection Vulkan branches. See
    // ToVkFrontFace(): the winding must be inverted alongside this flip.
    viewport.y = y + h;
    viewport.width = w;
    viewport.height = -h;
    viewport.minDepth = zmin;
    viewport.maxDepth = zmax;
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
}

void VulkanCommandList::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    ShellCommandListBase::SetScissor(x, y, w, h);

    VkRect2D scissor{};
    scissor.offset = {x, y};
    scissor.extent = {w, h};
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
}

void VulkanCommandList::Dispatch(uint32_t, uint32_t, uint32_t)
{
    RTRLAB_ASSERT_MSG(false,
                      "Vulkan dispatch is not implemented yet. This backend no longer falls back to the shell "
                      "compute path.");
}

void VulkanCommandList::TextureBarrier(
    Texture* texture, TextureState oldState, TextureState newState, ShaderStage srcStage, ShaderStage dstStage)
{
    if (texture == nullptr || oldState == newState)
        return;

    const TextureDesc& desc = texture->GetDesc();
    const TextureState trackedState = GetTrackedTextureState(texture);
    RTRLAB_ASSERTF(trackedState == oldState,
                   "Vulkan TextureBarrier expected old state {} but tracked state is {}.",
                   static_cast<uint32_t>(oldState),
                   static_cast<uint32_t>(trackedState));

    const VkImageLayout oldLayout = GetTrackedImageLayout(texture);
    const VkImageLayout expectedOldLayout = ToVkImageLayout(oldState, desc);
    RTRLAB_ASSERT_MSG(oldLayout == expectedOldLayout,
                      "Vulkan TextureBarrier found a layout/state mismatch before recording the barrier.");

    const VkImageLayout newLayout = ToVkImageLayout(newState, desc);
    TransitionImageLayout(m_CommandBuffer,
                          GetVkImageForBarrier(texture),
                          oldLayout,
                          newLayout,
                          TextureStateToPipelineStageMask(oldState, srcStage, false),
                          TextureStateToPipelineStageMask(newState, dstStage, true),
                          TextureStateToAccessMask(oldState),
                          TextureStateToAccessMask(newState),
                          ToVkImageAspect(GetFullTextureAspect(desc), desc.m_Format),
                          std::max(desc.m_MipLevels, 1u),
                          std::max(desc.m_ArrayLayers, 1u));

    SetTrackedTextureState(texture, newState, newLayout);
}

void VulkanCommandList::BufferBarrier(
    Buffer* buffer, BufferState oldState, BufferState newState, ShaderStage srcStage, ShaderStage dstStage)
{
    if (buffer == nullptr || oldState == newState)
        return;

    VulkanBuffer& vulkanBuffer = GetVulkanBuffer(buffer);
    RTRLAB_ASSERTF(vulkanBuffer.GetCurrentState() == oldState,
                   "Vulkan BufferBarrier expected old state {} but tracked state is {}.",
                   static_cast<uint32_t>(oldState),
                   static_cast<uint32_t>(vulkanBuffer.GetCurrentState()));

    VkBufferMemoryBarrier barrier = MakeVkStruct<VkBufferMemoryBarrier, VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER>();
    barrier.srcAccessMask = BufferStateToAccessMask(oldState);
    barrier.dstAccessMask = BufferStateToAccessMask(newState);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = vulkanBuffer.GetVkBuffer();
    barrier.offset = 0;
    barrier.size = vulkanBuffer.GetDesc().m_Size;

    vkCmdPipelineBarrier(m_CommandBuffer,
                         BufferStateToPipelineStageMask(oldState, srcStage),
                         BufferStateToPipelineStageMask(newState, dstStage),
                         0,
                         0,
                         nullptr,
                         1,
                         &barrier,
                         0,
                         nullptr);

    vulkanBuffer.SetCurrentState(newState);
}

void VulkanCommandList::Draw(uint32_t vertexCount, uint32_t firstVertex)
{
    ShellCommandListBase::Draw(vertexCount, firstVertex);
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan Draw requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Vulkan Draw requires a bound graphics pipeline.");
    vkCmdDraw(m_CommandBuffer, vertexCount, 1, firstVertex, 0);
}

void VulkanCommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
{
    ShellCommandListBase::DrawIndexed(indexCount, firstIndex, vertexOffset);
    RTRLAB_ASSERT_MSG(m_IsRendering, "Vulkan DrawIndexed requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Vulkan DrawIndexed requires a bound graphics pipeline.");
    RTRLAB_ASSERT_MSG(m_IndexBuffer != nullptr, "Vulkan DrawIndexed requires a bound index buffer.");
    vkCmdDrawIndexed(m_CommandBuffer, indexCount, 1, firstIndex, vertexOffset, 0);
}

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device,
                                 const SwapchainDesc& desc,
                                 const NativeWindowHandle& nativeWindowHandle)
    : m_Device(device), m_Desc(RHIInternal::SanitizeSwapchainDesc(desc)), m_NativeWindowHandle(nativeWindowHandle)
{
    RTRLAB_ASSERT_MSG(RHIInternal::IsNativeWindowHandleValid(nativeWindowHandle),
                      "Native window handle is incomplete.");

    RecreateSwapchain();
}

VulkanSwapchain::~VulkanSwapchain()
{
    DestroySwapchain();
}

uint32_t VulkanSwapchain::AcquireNextImage()
{
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(m_Device.GetVkDevice(),
                                            m_Swapchain,
                                            std::numeric_limits<uint64_t>::max(),
                                            m_Device.GetCurrentImageAvailableSemaphore(),
                                            VK_NULL_HANDLE,
                                            &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain(m_Swapchain);
        result = vkAcquireNextImageKHR(m_Device.GetVkDevice(),
                                       m_Swapchain,
                                       std::numeric_limits<uint64_t>::max(),
                                       m_Device.GetCurrentImageAvailableSemaphore(),
                                       VK_NULL_HANDLE,
                                       &imageIndex);
    }

    if (result != VK_SUBOPTIMAL_KHR)
        CheckVk(result, "vkAcquireNextImageKHR");
    return imageIndex;
}

Texture* VulkanSwapchain::GetImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain image index out of range.");
    return m_Images[imageIndex].get();
}

TextureView* VulkanSwapchain::GetImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex < m_ImageViews.size(), "Swapchain image-view index out of range.");
    return m_ImageViews[imageIndex].get();
}

void VulkanSwapchain::Present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex < m_Images.size(), "Swapchain present index out of range.");

    VkPresentInfoKHR presentInfo = MakeVkStruct<VkPresentInfoKHR, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR>();
    const VkSemaphore renderFinishedSemaphore = m_Device.GetCurrentRenderFinishedSemaphore();
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_Device.GetPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain(m_Swapchain);
        m_Device.RecycleCurrentRenderFinishedSemaphore();
        m_Device.AdvanceFrameSync();
        return;
    }

    CheckVk(result, "vkQueuePresentKHR");
    m_Device.AdvanceFrameSync();
}

void VulkanSwapchain::Resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.m_Width = std::max(newWidth, 1u);
    m_Desc.m_Height = std::max(newHeight, 1u);
    RecreateSwapchain(m_Swapchain);
}

TextureDesc VulkanSwapchain::BuildSwapchainImageDesc() const
{
    TextureDesc textureDesc;
    textureDesc.m_Type = TextureType::Tex2D;
    textureDesc.m_Format = m_Desc.m_Format;
    textureDesc.m_Extent = Extent3D{m_Desc.m_Width, m_Desc.m_Height, 1};
    textureDesc.m_MipLevels = 1;
    textureDesc.m_ArrayLayers = 1;
    textureDesc.m_UsageMask = TextureUsage::RenderTarget;
    textureDesc.m_DebugName = "VulkanSwapchainImage";
    return textureDesc;
}

void VulkanSwapchain::RecreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    const VkPhysicalDevice physicalDevice = m_Device.GetVkPhysicalDevice();
    const VkSurfaceKHR surface = m_Device.GetVkSurface();
    const VkDevice device = m_Device.GetVkDevice();

    if (oldSwapchain != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device);

    VkSurfaceCapabilitiesKHR capabilities{};
    CheckVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(physicalDevice, surface, m_Desc.m_Format);
    const VkPresentModeKHR presentMode = ChoosePresentMode(physicalDevice, surface, m_Desc.m_Vsync);
    const VkExtent2D extent = ChooseSwapchainExtent(capabilities, m_Desc.m_Width, m_Desc.m_Height);

    uint32_t desiredImageCount = std::max(m_Desc.m_ImageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0)
        desiredImageCount = std::min(desiredImageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo =
        MakeVkStruct<VkSwapchainCreateInfoKHR, VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR>();
    createInfo.surface = surface;
    createInfo.minImageCount = desiredImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const uint32_t queueFamilyIndices[] = {m_Device.GetGraphicsQueueFamily(), m_Device.GetPresentQueueFamily()};

    if (m_Device.GetGraphicsQueueFamily() != m_Device.GetPresentQueueFamily())
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    CheckVk(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain), "vkCreateSwapchainKHR");

    uint32_t swapchainImageCount = 0;
    CheckVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, nullptr),
            "vkGetSwapchainImagesKHR(count)");
    RTRLAB_ASSERT_MSG(swapchainImageCount > 0, "Vulkan swapchain reported zero images.");

    std::vector<VkImage> swapchainImages(swapchainImageCount);
    CheckVk(vkGetSwapchainImagesKHR(device, newSwapchain, &swapchainImageCount, swapchainImages.data()),
            "vkGetSwapchainImagesKHR(list)");

    std::vector<Scope<VulkanSwapchainTexture>> images;
    std::vector<Scope<VulkanSwapchainImageView>> imageViews;
    images.reserve(swapchainImageCount);
    imageViews.reserve(swapchainImageCount);

    m_Desc.m_Width = extent.width;
    m_Desc.m_Height = extent.height;
    m_Desc.m_ImageCount = swapchainImageCount;
    m_Desc.m_Format = ToRhiFormat(surfaceFormat.format);
    if (m_Desc.m_Format == Format::Unknown)
        m_Desc.m_Format = Format::BGRA8_UNORM;

    const TextureDesc imageDesc = BuildSwapchainImageDesc();

    for (VkImage image : swapchainImages)
    {
        auto swapchainTexture = CreateScope<VulkanSwapchainTexture>(image, imageDesc);
        SetVulkanDebugName(device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), imageDesc.m_DebugName);

        VkImageViewCreateInfo viewCreateInfo =
            MakeVkStruct<VkImageViewCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO>();
        viewCreateInfo.image = image;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = surfaceFormat.format;
        viewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY};
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        VkImageView imageView = VK_NULL_HANDLE;
        CheckVk(vkCreateImageView(device, &viewCreateInfo, nullptr, &imageView), "vkCreateImageView");
        SetVulkanDebugName(
            device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), "VulkanSwapchainImage.View");

        TextureViewDesc viewDesc;
        viewDesc.m_Type = TextureType::Tex2D;
        viewDesc.m_Format = m_Desc.m_Format;
        viewDesc.m_Aspect = TextureAspect::Color;

        imageViews.push_back(
            CreateScope<VulkanSwapchainImageView>(device, swapchainTexture.get(), imageView, viewDesc));
        images.push_back(std::move(swapchainTexture));
    }

    DestroySwapchain();

    m_Swapchain = newSwapchain;
    m_VkFormat = surfaceFormat.format;
    m_Images = std::move(images);
    m_ImageViews = std::move(imageViews);
}

void VulkanSwapchain::DestroySwapchain()
{
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Device.GetVkDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

VulkanDevice::VulkanDevice()
{
    CheckVk(volkInitialize(), "volkInitialize");
    InitializeInstance();
}

VulkanDevice::~VulkanDevice()
{
    ShutdownPresentationObjects();
    ShutdownDeviceObjects();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

Scope<Swapchain> VulkanDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    InitializePresentationObjects(nativeWindowHandle);
    return CreateScope<VulkanSwapchain>(*this, desc, nativeWindowHandle);
}

Scope<Buffer> VulkanDevice::CreateBuffer(const BufferDesc& desc)
{
    InitializeDeviceObjects();
    RTRLAB_ASSERT_MSG(m_Allocator != nullptr, "Vulkan buffer allocation requires an initialized VMA allocator.");
    const BufferDesc sanitizedDesc = RHIInternal::SanitizeBufferDesc(desc);

    VkBufferCreateInfo createInfo = MakeVkStruct<VkBufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO>();
    createInfo.size = sanitizedDesc.m_Size;
    createInfo.usage = ToVkBufferUsage(sanitizedDesc.m_UsageMask);
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = ToVmaMemoryUsage(sanitizedDesc.m_MemoryUsage);
    allocationCreateInfo.flags = ToVmaAllocationCreateFlags(sanitizedDesc.m_MemoryUsage);

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    CheckVk(vmaCreateBuffer(m_Allocator, &createInfo, &allocationCreateInfo, &buffer, &allocation, nullptr),
            "vmaCreateBuffer");
    SetVulkanDebugName(m_Device, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer), sanitizedDesc.m_DebugName);

    return CreateScope<VulkanBuffer>(m_Allocator, buffer, allocation, sanitizedDesc);
}

Scope<Texture> VulkanDevice::CreateTexture(const TextureDesc& desc)
{
    // TRANSITIONAL(M3): TextureDesc does not expose residency policy yet, so the
    // backend still creates optimal-tiled device-local images by default even
    // though allocation is now routed through VMA.
    InitializeDeviceObjects();
    RTRLAB_ASSERT_MSG(m_Allocator != nullptr, "Vulkan texture allocation requires an initialized VMA allocator.");
    const TextureDesc sanitizedDesc = RHIInternal::SanitizeTextureDesc(desc);

    VkImageCreateInfo createInfo = MakeVkStruct<VkImageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO>();
    createInfo.imageType = ToVkImageType(sanitizedDesc.m_Type);
    createInfo.format = ToVkFormat(sanitizedDesc.m_Format);
    createInfo.extent =
        VkExtent3D{sanitizedDesc.m_Extent.m_Width, sanitizedDesc.m_Extent.m_Height, sanitizedDesc.m_Extent.m_Depth};
    createInfo.mipLevels = sanitizedDesc.m_MipLevels;
    createInfo.arrayLayers = sanitizedDesc.m_ArrayLayers;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = ToVkImageUsage(sanitizedDesc.m_UsageMask);
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (sanitizedDesc.m_Type == TextureType::Cube)
    {
        createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    CheckVk(vmaCreateImage(m_Allocator, &createInfo, &allocationCreateInfo, &image, &allocation, nullptr),
            "vmaCreateImage");
    SetVulkanDebugName(m_Device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image), sanitizedDesc.m_DebugName);

    return CreateScope<VulkanTexture>(m_Allocator, image, allocation, sanitizedDesc);
}

Scope<TextureView> VulkanDevice::CreateTextureView(Texture* texture, const TextureViewDesc& desc)
{
    InitializeDeviceObjects();
    RTRLAB_ASSERT_MSG(texture != nullptr, "Vulkan CreateTextureView requires a valid texture.");

    const TextureDesc& textureDesc = texture->GetDesc();
    const Format viewFormat = desc.m_Format == Format::Unknown ? textureDesc.m_Format : desc.m_Format;
    const uint32_t mipLevelCount =
        desc.m_MipLevelCount == 0 ? std::max(textureDesc.m_MipLevels - desc.m_BaseMipLevel, 1u) : desc.m_MipLevelCount;
    const uint32_t arrayLayerCount = desc.m_ArrayLayerCount == 0
                                         ? std::max(textureDesc.m_ArrayLayers - desc.m_BaseArrayLayer, 1u)
                                         : desc.m_ArrayLayerCount;

    VkImageViewCreateInfo createInfo = MakeVkStruct<VkImageViewCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO>();
    createInfo.image = GetVkImageFromOwnedTexture(texture);
    createInfo.viewType = ToVkImageViewType(desc.m_Type);
    createInfo.format = ToVkFormat(viewFormat);
    createInfo.subresourceRange.aspectMask = ToVkImageAspect(desc.m_Aspect, viewFormat);
    createInfo.subresourceRange.baseMipLevel = desc.m_BaseMipLevel;
    createInfo.subresourceRange.levelCount = mipLevelCount;
    createInfo.subresourceRange.baseArrayLayer = desc.m_BaseArrayLayer;
    createInfo.subresourceRange.layerCount = arrayLayerCount;

    VkImageView imageView = VK_NULL_HANDLE;
    CheckVk(vkCreateImageView(m_Device, &createInfo, nullptr, &imageView), "vkCreateImageView(texture)");
    const std::string debugName = MakeTextureViewDebugName(*texture);
    SetVulkanDebugName(m_Device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageView), debugName.c_str());

    TextureViewDesc resolvedDesc = desc;
    resolvedDesc.m_Format = viewFormat;
    resolvedDesc.m_MipLevelCount = mipLevelCount;
    resolvedDesc.m_ArrayLayerCount = arrayLayerCount;
    return CreateScope<VulkanTextureView>(m_Device, texture, imageView, resolvedDesc);
}

Scope<Sampler> VulkanDevice::CreateSampler(const SamplerDesc& desc)
{
    InitializeDeviceObjects();

    VkSamplerCreateInfo createInfo = MakeVkStruct<VkSamplerCreateInfo, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO>();
    createInfo.magFilter = ToVkFilter(desc.m_MagFilter);
    createInfo.minFilter = ToVkFilter(desc.m_MinFilter);
    createInfo.mipmapMode = ToVkMipmapMode(desc.m_MipFilter);
    createInfo.addressModeU = ToVkAddressMode(desc.m_AddressU);
    createInfo.addressModeV = ToVkAddressMode(desc.m_AddressV);
    createInfo.addressModeW = ToVkAddressMode(desc.m_AddressW);
    createInfo.mipLodBias = desc.m_MipLodBias;
    createInfo.anisotropyEnable = desc.m_AnisotropyEnable ? VK_TRUE : VK_FALSE;
    createInfo.maxAnisotropy = std::max(desc.m_MaxAnisotropy, 1.0f);
    // TRANSITIONAL(M4): SamplerDesc does not yet expose compare-op or border-color
    // controls. Vulkan shadow samplers and explicit border colors will be wired
    // through the future shader-system sampler contract instead of this fixed
    // M3 bring-up state.
    createInfo.compareEnable = VK_FALSE;
    createInfo.minLod = desc.m_MinLod;
    createInfo.maxLod = desc.m_MaxLod;
    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler = VK_NULL_HANDLE;
    CheckVk(vkCreateSampler(m_Device, &createInfo, nullptr, &sampler), "vkCreateSampler");
    return CreateScope<VulkanSampler>(m_Device, sampler, desc);
}

Scope<ShaderProgram> VulkanDevice::CreateShaderProgram(const CompiledShaderProgramDesc& desc)
{
    InitializeDeviceObjects();

    std::vector<VulkanShaderProgram::StageModule> modules;
    modules.reserve(desc.m_Blobs.size());

    for (const CompiledShaderBlob& blob : desc.m_Blobs)
    {
        if (blob.m_Backend != BackendType::Vulkan)
            continue;

        RTRLAB_ASSERT_MSG(!blob.m_Code.empty(), "Vulkan shader blobs must contain SPIR-V bytes.");
        RTRLAB_ASSERT_MSG(!blob.m_EntryPoint.empty(), "Vulkan shader blobs must carry an entry-point name.");
        RTRLAB_ASSERT_MSG((blob.m_Code.size() % sizeof(uint32_t)) == 0,
                          "Vulkan shader blobs must contain aligned SPIR-V words.");

        VkShaderModuleCreateInfo createInfo =
            MakeVkStruct<VkShaderModuleCreateInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO>();
        createInfo.codeSize = blob.m_Code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(blob.m_Code.data());

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        CheckVk(vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule), "vkCreateShaderModule");
        modules.push_back({blob.m_Stage, blob.m_EntryPoint, shaderModule});
    }

    RTRLAB_ASSERT_MSG(!modules.empty(), "Vulkan CreateShaderProgram requires at least one Vulkan shader blob.");
    return CreateScope<VulkanShaderProgram>(m_Device, desc, std::move(modules));
}

Scope<PipelineLayout> VulkanDevice::CreatePipelineLayout(const PipelineLayoutDesc& desc)
{
    InitializeDeviceObjects();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = CreateVkDescriptorSetLayouts(m_Device, desc);
    VkPipelineLayout pipelineLayout = CreateVkPipelineLayout(m_Device, desc, descriptorSetLayouts);
    return CreateScope<VulkanPipelineLayout>(m_Device, desc, pipelineLayout, std::move(descriptorSetLayouts));
}

Scope<ResourceSet> VulkanDevice::CreateResourceSet(PipelineLayout* layout, uint32_t setIndex)
{
    InitializeDeviceObjects();

    VulkanPipelineLayout& pipelineLayout = GetVulkanPipelineLayout(layout);
    const std::vector<const BindingInfo*> setBindings =
        RHIInternal::CollectBindingInfosForSet(pipelineLayout.GetDesc(), setIndex);
    RTRLAB_ASSERTF(
        !setBindings.empty(), "Vulkan CreateResourceSet requires a valid set {} in the PipelineLayout.", setIndex);

    VkDescriptorPool descriptorPool = CreateVkDescriptorPoolForSet(m_Device, pipelineLayout.GetDesc(), setIndex);
    VkDescriptorSet descriptorSet = AllocateVkDescriptorSet(m_Device, descriptorPool, pipelineLayout, setIndex);
    return CreateScope<VulkanResourceSet>(
        m_Device, layout, setIndex, descriptorPool, descriptorSet, static_cast<uint32_t>(m_FrameUploadArenas.size()));
}

Scope<VertexInputLayout> VulkanDevice::CreateVertexInputLayout(const VertexInputLayoutDesc& desc)
{
    std::vector<VkVertexInputBindingDescription> bindings;
    bindings.reserve(desc.m_Buffers.size());
    for (uint32_t bindingIndex = 0; bindingIndex < static_cast<uint32_t>(desc.m_Buffers.size()); ++bindingIndex)
    {
        const VertexBufferLayoutDesc& buffer = desc.m_Buffers[bindingIndex];
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = bindingIndex;
        bindingDescription.stride = buffer.m_Stride;
        bindingDescription.inputRate =
            buffer.m_PerInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(bindingDescription);
    }

    std::vector<VkVertexInputAttributeDescription> attributes;
    attributes.reserve(desc.m_Attributes.size());
    for (const VertexAttributeDesc& attribute : desc.m_Attributes)
    {
        RTRLAB_ASSERT_MSG(attribute.m_BufferSlot < desc.m_Buffers.size(),
                          "Vertex attribute buffer slot must reference an existing vertex buffer layout.");

        VkVertexInputAttributeDescription attributeDescription{};
        attributeDescription.location = attribute.m_Location;
        attributeDescription.binding = attribute.m_BufferSlot;
        attributeDescription.format = ToVkFormat(attribute.m_Format);
        attributeDescription.offset = attribute.m_Offset;
        attributes.push_back(attributeDescription);
    }

    return CreateScope<VulkanVertexInputLayout>(desc, std::move(bindings), std::move(attributes));
}

Scope<GraphicsPipeline> VulkanDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    InitializeDeviceObjects();

    RTRLAB_ASSERT_MSG(desc.m_PipelineLayout != nullptr, "Vulkan graphics pipelines require a PipelineLayout.");
    RTRLAB_ASSERT_MSG(desc.m_ShaderProgram != nullptr, "Vulkan graphics pipelines require a ShaderProgram.");
    RTRLAB_ASSERT_MSG(desc.m_VertexInput != nullptr, "Vulkan graphics pipelines require a VertexInputLayout.");
    RTRLAB_ASSERT_MSG(!desc.m_ColorFormats.empty() || desc.m_DepthFormat != Format::Unknown,
                      "Vulkan graphics pipelines require at least one render-target format.");

    const VulkanShaderProgram& shaderProgram = GetVulkanShaderProgram(desc.m_ShaderProgram);
    const VulkanVertexInputLayout& vertexInput = GetVulkanVertexInputLayout(desc.m_VertexInput);
    VulkanPipelineLayout& pipelineLayout = GetVulkanPipelineLayout(desc.m_PipelineLayout);

    const VulkanShaderProgram::StageModule* vertexStage = shaderProgram.FindStage(ShaderStage::Vertex);
    const VulkanShaderProgram::StageModule* fragmentStage = shaderProgram.FindStage(ShaderStage::Fragment);
    RTRLAB_ASSERT_MSG(vertexStage != nullptr, "Vulkan graphics pipelines require a vertex shader stage.");
    RTRLAB_ASSERT_MSG(fragmentStage != nullptr, "Vulkan graphics pipelines require a fragment shader stage.");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexStage->m_Module;
    shaderStages[0].pName = vertexStage->m_EntryPoint.c_str();
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentStage->m_Module;
    shaderStages[1].pName = fragmentStage->m_EntryPoint.c_str();

    VkPipelineVertexInputStateCreateInfo vertexInputState =
        MakeVkStruct<VkPipelineVertexInputStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO>();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInput.GetVkBindings().size());
    vertexInputState.pVertexBindingDescriptions = vertexInput.GetVkBindings().data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInput.GetVkAttributes().size());
    vertexInputState.pVertexAttributeDescriptions = vertexInput.GetVkAttributes().data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        MakeVkStruct<VkPipelineInputAssemblyStateCreateInfo,
                     VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO>();
    inputAssemblyState.topology = ToVkPrimitiveTopology(desc.m_Topology);
    inputAssemblyState.primitiveRestartEnable =
        desc.m_Topology == PrimitiveTopology::TriangleStrip || desc.m_Topology == PrimitiveTopology::LineStrip;

    VkPipelineViewportStateCreateInfo viewportState =
        MakeVkStruct<VkPipelineViewportStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO>();
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        MakeVkStruct<VkPipelineRasterizationStateCreateInfo,
                     VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO>();
    rasterizationState.depthClampEnable = desc.m_RasterState.m_DepthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = ToVkPolygonMode(desc.m_RasterState.m_FillMode);
    rasterizationState.cullMode = ToVkCullMode(desc.m_RasterState.m_CullMode);
    rasterizationState.frontFace = ToVkFrontFace(desc.m_RasterState.m_FrontFace);
    rasterizationState.depthBiasEnable = desc.m_RasterState.m_DepthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizationState.depthBiasConstantFactor = desc.m_RasterState.m_DepthBiasConstant;
    rasterizationState.depthBiasSlopeFactor = desc.m_RasterState.m_DepthBiasSlopeFactor;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState =
        MakeVkStruct<VkPipelineMultisampleStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO>();
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        MakeVkStruct<VkPipelineDepthStencilStateCreateInfo,
                     VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO>();
    depthStencilState.depthTestEnable = desc.m_DepthStencilState.m_DepthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthWriteEnable = desc.m_DepthStencilState.m_DepthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthCompareOp = ToVkCompareOp(desc.m_DepthStencilState.m_DepthCompareOp);

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(desc.m_ColorFormats.size());
    for (VkPipelineColorBlendAttachmentState& colorBlendAttachment : colorBlendAttachments)
    {
        colorBlendAttachment.blendEnable = desc.m_BlendState.m_BlendEnable ? VK_TRUE : VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_SrcColorFactor);
        colorBlendAttachment.dstColorBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_DstColorFactor);
        colorBlendAttachment.colorBlendOp = ToVkBlendOp(desc.m_BlendState.m_ColorBlendOp);
        colorBlendAttachment.srcAlphaBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_SrcAlphaFactor);
        colorBlendAttachment.dstAlphaBlendFactor = ToVkBlendFactor(desc.m_BlendState.m_DstAlphaFactor);
        colorBlendAttachment.alphaBlendOp = ToVkBlendOp(desc.m_BlendState.m_AlphaBlendOp);
        colorBlendAttachment.colorWriteMask = ToVkColorWriteMask(desc.m_BlendState.m_ColorWriteMask);
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        MakeVkStruct<VkPipelineColorBlendStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO>();
    colorBlendState.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlendState.pAttachments = colorBlendAttachments.data();

    const std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState =
        MakeVkStruct<VkPipelineDynamicStateCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO>();
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    std::vector<VkFormat> colorAttachmentFormats;
    colorAttachmentFormats.reserve(desc.m_ColorFormats.size());
    for (Format colorFormat : desc.m_ColorFormats)
        colorAttachmentFormats.push_back(ToVkFormat(colorFormat));

    VkPipelineRenderingCreateInfo renderingInfo =
        MakeVkStruct<VkPipelineRenderingCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO>();
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
    renderingInfo.depthAttachmentFormat =
        desc.m_DepthFormat == Format::Unknown ? VK_FORMAT_UNDEFINED : ToVkFormat(desc.m_DepthFormat);
    renderingInfo.stencilAttachmentFormat =
        HasStencilComponent(desc.m_DepthFormat) ? ToVkFormat(desc.m_DepthFormat) : VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo createInfo =
        MakeVkStruct<VkGraphicsPipelineCreateInfo, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO>();
    createInfo.pNext = &renderingInfo;
    createInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    createInfo.pStages = shaderStages.data();
    createInfo.pVertexInputState = &vertexInputState;
    createInfo.pInputAssemblyState = &inputAssemblyState;
    createInfo.pViewportState = &viewportState;
    createInfo.pRasterizationState = &rasterizationState;
    createInfo.pMultisampleState = &multisampleState;
    createInfo.pDepthStencilState = desc.m_DepthFormat == Format::Unknown ? nullptr : &depthStencilState;
    createInfo.pColorBlendState = &colorBlendState;
    createInfo.pDynamicState = &dynamicState;
    createInfo.layout = pipelineLayout.GetVkPipelineLayout();

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS)
        CheckVk(result, "vkCreateGraphicsPipelines");
    return CreateScope<VulkanGraphicsPipeline>(m_Device, desc, &pipelineLayout, pipeline);
}

Scope<ComputePipeline> VulkanDevice::CreateComputePipeline(const ComputePipelineDesc&)
{
    RTRLAB_ASSERT_MSG(false,
                      "Vulkan compute pipelines are not implemented yet. This backend no longer creates shell "
                      "compute-pipeline placeholders.");
    return nullptr;
}

void VulkanDevice::WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size)
{
    // TRANSITIONAL(M3): Demo-only direct host upload path for early bring-up.
    InitializeDeviceObjects();
    RTRLAB_ASSERT_MSG(m_Allocator != nullptr, "Vulkan WriteBuffer requires an initialized VMA allocator.");

    if (size == 0)
        return;

    RTRLAB_ASSERT_MSG(buffer != nullptr, "Vulkan WriteBuffer requires a valid buffer.");
    RTRLAB_ASSERT_MSG(data != nullptr, "Vulkan WriteBuffer requires non-null source data.");

    VulkanBuffer& vulkanBuffer = GetVulkanBuffer(buffer);
    const BufferDesc& desc = vulkanBuffer.GetDesc();
    RTRLAB_ASSERT_MSG(desc.m_MemoryUsage == MemoryUsage::CpuToGpu,
                      "Vulkan WriteBuffer currently requires a CpuToGpu buffer.");
    RTRLAB_ASSERT_MSG(offset + size <= desc.m_Size, "Vulkan WriteBuffer range exceeds the buffer size.");

    void* mappedData = nullptr;
    CheckVk(vmaMapMemory(m_Allocator, vulkanBuffer.GetVmaAllocation(), &mappedData), "vmaMapMemory");
    std::memcpy(static_cast<std::byte*>(mappedData) + offset, data, static_cast<size_t>(size));
    CheckVk(vmaFlushAllocation(m_Allocator, vulkanBuffer.GetVmaAllocation(), offset, size), "vmaFlushAllocation");
    vmaUnmapMemory(m_Allocator, vulkanBuffer.GetVmaAllocation());
}

CommandList* VulkanDevice::BeginCommandList()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before command recording.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan command recording requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_FrameSubmitted, "Vulkan command recording must happen before queue submission.");

    VkCommandBuffer commandBuffer = m_CommandList.GetVkCommandBuffer();
    CheckVk(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");

    VkCommandBufferBeginInfo beginInfo =
        MakeVkStruct<VkCommandBufferBeginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO>();
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    return &m_CommandList;
}

void VulkanDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before queue submission.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan Submit requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_FrameSubmitted, "Vulkan Submit must only happen once per frame in the current bring-up path.");
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "VulkanDevice only accepts submissions from its backend command list.");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(),
                      "Vulkan Submit requires EndRendering before queue submission.");

    const VkCommandBuffer commandBuffer = m_CommandList.GetVkCommandBuffer();
    CheckVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

    const FrameSync& frameSync = GetCurrentFrameSync();
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = MakeVkStruct<VkSubmitInfo, VK_STRUCTURE_TYPE_SUBMIT_INFO>();
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameSync.m_ImageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameSync.m_RenderFinished;

    CheckVk(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, frameSync.m_InFlightFence), "vkQueueSubmit");
    m_FrameSubmitted = true;
}

FrameContext* VulkanDevice::BeginFrame()
{
    RTRLAB_ASSERT_MSG(m_HasPresentationObjects,
                      "Vulkan presentation objects must be initialized before beginning a frame.");
    RTRLAB_ASSERT_MSG(!m_FrameInProgress, "VulkanDevice does not support nested frame lifetimes.");

    FrameSync& frameSync = GetCurrentFrameSync();
    CheckVk(vkWaitForFences(m_Device, 1, &frameSync.m_InFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max()),
            "vkWaitForFences");
    CheckVk(vkResetFences(m_Device, 1, &frameSync.m_InFlightFence), "vkResetFences");
    ResetCurrentFrameUploadArena();

    m_FrameInProgress = true;
    m_FrameSubmitted = false;
    return &m_FrameContext;
}

void VulkanDevice::EndFrame(FrameContext* frameContext)
{
    RTRLAB_ASSERT_MSG(frameContext == &m_FrameContext, "VulkanDevice only accepts its backend frame context.");
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Vulkan EndFrame requires an active frame.");
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Vulkan EndFrame currently expects queue submission before the frame closes.");

    m_FrameInProgress = false;
}

VkSemaphore VulkanDevice::GetCurrentImageAvailableSemaphore() const
{
    RTRLAB_ASSERT_MSG(m_FrameInProgress, "Current Vulkan frame sync is only valid during an active frame.");
    return GetCurrentFrameSync().m_ImageAvailable;
}

VkSemaphore VulkanDevice::GetCurrentRenderFinishedSemaphore() const
{
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Render-finished semaphore is only valid after queue submission.");
    return GetCurrentFrameSync().m_RenderFinished;
}

void VulkanDevice::AdvanceFrameSync()
{
    // The current bring-up path only advances after a submitted frame is either presented
    // or discarded during swapchain recreation. If future acquire-failure handling starts
    // skipping frames before submission, this invariant will need to be revisited.
    RTRLAB_ASSERT_MSG(!m_FrameInProgress, "Vulkan frame sync can only advance after EndFrame.");
    RTRLAB_ASSERT_MSG(m_FrameSubmitted, "Vulkan frame sync expects queue submission before presentation advances.");

    m_CurrentFrameSlot = (m_CurrentFrameSlot + 1) % static_cast<uint32_t>(m_FrameSyncObjects.size());
    m_FrameSubmitted = false;
}

VulkanDevice::FrameSync& VulkanDevice::GetCurrentFrameSync()
{
    return m_FrameSyncObjects[m_CurrentFrameSlot];
}

const VulkanDevice::FrameSync& VulkanDevice::GetCurrentFrameSync() const
{
    return m_FrameSyncObjects[m_CurrentFrameSlot];
}

void VulkanDevice::RecycleCurrentRenderFinishedSemaphore()
{
    FrameSync& frameSync = GetCurrentFrameSync();

    // Safety relies on the caller first making the queue/device idle. The current present
    // failure path does this via RecreateSwapchain() before recycling the semaphore.
    if (frameSync.m_RenderFinished != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_Device, frameSync.m_RenderFinished, nullptr);
        frameSync.m_RenderFinished = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo =
        MakeVkStruct<VkSemaphoreCreateInfo, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO>();
    CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_RenderFinished),
            "vkCreateSemaphore(renderFinished recycle)");
}

void VulkanDevice::InitializeFrameUploadArenas()
{
    RTRLAB_ASSERT_MSG(m_Allocator != nullptr, "Vulkan frame upload arenas require an initialized VMA allocator.");

    for (FrameUploadArena& arena : m_FrameUploadArenas)
    {
        if (arena.m_Buffer != VK_NULL_HANDLE)
            continue;

        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = kFrameUploadArenaInitialCapacity;
        createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationCreateInfo{};
        allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationCreateInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo allocationInfo{};
        CheckVk(
            vmaCreateBuffer(
                m_Allocator, &createInfo, &allocationCreateInfo, &arena.m_Buffer, &arena.m_Allocation, &allocationInfo),
            "vmaCreateBuffer(frame upload arena)");

        arena.m_MappedData = allocationInfo.pMappedData;
        arena.m_RequiresUnmap = false;
        if (arena.m_MappedData == nullptr)
        {
            CheckVk(vmaMapMemory(m_Allocator, arena.m_Allocation, &arena.m_MappedData),
                    "vmaMapMemory(frame upload arena)");
            arena.m_RequiresUnmap = true;
        }

        arena.m_Capacity = createInfo.size;
        arena.m_Head = 0;
        arena.m_Serial = 0;
    }
}

void VulkanDevice::ShutdownFrameUploadArenas()
{
    if (m_Allocator == nullptr)
        return;

    for (FrameUploadArena& arena : m_FrameUploadArenas)
    {
        if (arena.m_Buffer != VK_NULL_HANDLE)
        {
            if (arena.m_MappedData != nullptr && arena.m_RequiresUnmap)
            {
                vmaUnmapMemory(m_Allocator, arena.m_Allocation);
                arena.m_MappedData = nullptr;
            }

            vmaDestroyBuffer(m_Allocator, arena.m_Buffer, arena.m_Allocation);
        }

        arena.m_Buffer = VK_NULL_HANDLE;
        arena.m_Allocation = nullptr;
        arena.m_MappedData = nullptr;
        arena.m_RequiresUnmap = false;
        arena.m_Capacity = 0;
        arena.m_Head = 0;
        arena.m_Serial = 0;
    }
}

void VulkanDevice::ResetCurrentFrameUploadArena()
{
    FrameUploadArena& arena = m_FrameUploadArenas[m_CurrentFrameSlot];
    RTRLAB_ASSERT_MSG(arena.m_Buffer != VK_NULL_HANDLE,
                      "Vulkan frame upload arena must be initialized before beginning a frame.");
    arena.m_Head = 0;
    ++arena.m_Serial;
}

void VulkanDevice::PrepareResourceSetForBinding(ResourceSet* resourceSet)
{
    if (resourceSet == nullptr)
        return;

    VulkanResourceSet& vulkanResourceSet = GetVulkanResourceSet(resourceSet);
    if (!vulkanResourceSet.HasConstantBinding())
        return;

    FrameUploadArena& arena = m_FrameUploadArenas[m_CurrentFrameSlot];
    RTRLAB_ASSERT_MSG(arena.m_Buffer != VK_NULL_HANDLE,
                      "Vulkan resource-set binding requires an initialized frame upload arena.");
    RTRLAB_ASSERT_MSG(arena.m_MappedData != nullptr,
                      "Vulkan resource-set binding requires the frame upload arena to be mapped.");

    if (!vulkanResourceSet.NeedsConstantUploadForFrame(m_CurrentFrameSlot, arena.m_Serial))
        return;

    const size_t constantDataSize = vulkanResourceSet.GetConstants().GetSize();
    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(std::max<size_t>(constantDataSize, 1));
    const VkDeviceSize alignedOffset = AlignUp(arena.m_Head, m_MinUniformBufferOffsetAlignment);
    RTRLAB_ASSERT_MSG(alignedOffset + uploadSize <= arena.m_Capacity,
                      "Vulkan frame upload arena ran out of space while preparing a ResourceSet.");

    std::byte* destination = static_cast<std::byte*>(arena.m_MappedData) + alignedOffset;
    if (constantDataSize > 0)
    {
        std::memcpy(destination, vulkanResourceSet.GetConstants().GetData(), constantDataSize);
    }
    else
    {
        destination[0] = std::byte{0};
    }

    CheckVk(vmaFlushAllocation(m_Allocator, arena.m_Allocation, alignedOffset, uploadSize),
            "vmaFlushAllocation(frame upload arena)");

    arena.m_Head = alignedOffset + uploadSize;
    vulkanResourceSet.WriteConstantDescriptorForFrame(
        m_CurrentFrameSlot, arena.m_Buffer, alignedOffset, uploadSize, arena.m_Serial);
}

void VulkanDevice::InitializeInstance()
{
    if (m_Instance != VK_NULL_HANDLE)
        return;

    const std::vector<const char*> instanceExtensions = GetSupportedInstanceExtensions();

    VkApplicationInfo appInfo = MakeVkStruct<VkApplicationInfo, VK_STRUCTURE_TYPE_APPLICATION_INFO>();
    appInfo.pApplicationName = "RTRLab";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "RTRLab";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = MakeVkStruct<VkInstanceCreateInfo, VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO>();
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_Instance), "vkCreateInstance");
    volkLoadInstance(m_Instance);
}

void VulkanDevice::InitializeDeviceObjects()
{
    if (m_HasDeviceObjects)
        return;

    InitializeInstance();

    uint32_t graphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    m_PhysicalDevice = PickPhysicalDevice(m_Instance, graphicsQueueFamily);
    m_GraphicsQueueFamily = graphicsQueueFamily;
    m_PresentQueueFamily = graphicsQueueFamily;

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo graphicsQueueCreateInfo =
        MakeVkStruct<VkDeviceQueueCreateInfo, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO>();
    graphicsQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;

    const std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceVulkan13Features vulkan13Features =
        MakeVkStruct<VkPhysicalDeviceVulkan13Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES>();
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = MakeVkStruct<VkDeviceCreateInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO>();
    deviceCreateInfo.pNext = &vulkan13Features;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &graphicsQueueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    CheckVk(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    VkPhysicalDeviceProperties physicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProperties);
    m_MinUniformBufferOffsetAlignment =
        std::max<uint64_t>(physicalDeviceProperties.limits.minUniformBufferOffsetAlignment, 1);

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    m_PresentQueue = m_GraphicsQueue;
    InitializeAllocator();
    InitializeFrameUploadArenas();

    VkCommandPoolCreateInfo commandPoolCreateInfo =
        MakeVkStruct<VkCommandPoolCreateInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO>();
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    CheckVk(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "vkCreateCommandPool");

    m_CommandList.Initialize(this, m_Device, m_CommandPool);
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasDeviceObjects = true;
}

void VulkanDevice::InitializeDeviceObjectsForSurface(VkSurfaceKHR surface)
{
    if (m_HasDeviceObjects)
    {
        RTRLAB_ASSERT_MSG(m_PhysicalDevice != VK_NULL_HANDLE, "Vulkan physical device must exist before presentation.");
        RTRLAB_ASSERT_MSG(SupportsPresentOnQueueFamily(m_PhysicalDevice, m_GraphicsQueueFamily, surface),
                          "This Vulkan device was initialized without a present-capable graphics queue for the "
                          "requested surface. Recreate the device from a swapchain-first path.");
        m_PresentQueueFamily = m_GraphicsQueueFamily;
        m_PresentQueue = m_GraphicsQueue;
        return;
    }

    InitializeInstance();

    QueueFamilySelection selection{};
    m_PhysicalDevice = PickPhysicalDevice(m_Instance, surface, selection);
    m_GraphicsQueueFamily = selection.m_GraphicsFamily;
    m_PresentQueueFamily = selection.m_PresentFamily;

    const float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo graphicsQueueCreateInfo =
        MakeVkStruct<VkDeviceQueueCreateInfo, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO>();
    graphicsQueueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    graphicsQueueCreateInfo.queueCount = 1;
    graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(graphicsQueueCreateInfo);

    if (m_PresentQueueFamily != m_GraphicsQueueFamily)
    {
        VkDeviceQueueCreateInfo presentQueueCreateInfo =
            MakeVkStruct<VkDeviceQueueCreateInfo, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO>();
        presentQueueCreateInfo.queueFamilyIndex = m_PresentQueueFamily;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(presentQueueCreateInfo);
    }

    const std::array<const char*, 1> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkPhysicalDeviceVulkan13Features vulkan13Features =
        MakeVkStruct<VkPhysicalDeviceVulkan13Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES>();
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = MakeVkStruct<VkDeviceCreateInfo, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO>();
    deviceCreateInfo.pNext = &vulkan13Features;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    CheckVk(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "vkCreateDevice");
    volkLoadDevice(m_Device);

    VkPhysicalDeviceProperties physicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProperties);
    m_MinUniformBufferOffsetAlignment =
        std::max<uint64_t>(physicalDeviceProperties.limits.minUniformBufferOffsetAlignment, 1);

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);
    InitializeAllocator();
    InitializeFrameUploadArenas();

    VkCommandPoolCreateInfo commandPoolCreateInfo =
        MakeVkStruct<VkCommandPoolCreateInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO>();
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    CheckVk(vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool), "vkCreateCommandPool");

    m_CommandList.Initialize(this, m_Device, m_CommandPool);
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasDeviceObjects = true;
}

void VulkanDevice::InitializeFrameSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo =
        MakeVkStruct<VkSemaphoreCreateInfo, VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO>();

    VkFenceCreateInfo fenceCreateInfo = MakeVkStruct<VkFenceCreateInfo, VK_STRUCTURE_TYPE_FENCE_CREATE_INFO>();
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (FrameSync& frameSync : m_FrameSyncObjects)
    {
        CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_ImageAvailable),
                "vkCreateSemaphore(imageAvailable)");
        CheckVk(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &frameSync.m_RenderFinished),
                "vkCreateSemaphore(renderFinished)");
        CheckVk(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &frameSync.m_InFlightFence),
                "vkCreateFence(inFlight)");
    }
}

void VulkanDevice::InitializeAllocator()
{
    if (m_Allocator != nullptr)
        return;

    RTRLAB_ASSERT_MSG(m_Instance != VK_NULL_HANDLE, "Vulkan VMA allocator requires a valid instance.");
    RTRLAB_ASSERT_MSG(m_PhysicalDevice != VK_NULL_HANDLE, "Vulkan VMA allocator requires a physical device.");
    RTRLAB_ASSERT_MSG(m_Device != VK_NULL_HANDLE, "Vulkan VMA allocator requires a logical device.");

    VmaAllocatorCreateInfo allocatorCreateInfo{};
    allocatorCreateInfo.instance = m_Instance;
    allocatorCreateInfo.physicalDevice = m_PhysicalDevice;
    allocatorCreateInfo.device = m_Device;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaVulkanFunctions vulkanFunctions{};
    CheckVk(vmaImportVulkanFunctionsFromVolk(&allocatorCreateInfo, &vulkanFunctions),
            "vmaImportVulkanFunctionsFromVolk");
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    CheckVk(vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator), "vmaCreateAllocator");
}

void VulkanDevice::ShutdownAllocator()
{
    if (m_Allocator == nullptr)
        return;

    vmaDestroyAllocator(m_Allocator);
    m_Allocator = nullptr;
}

void VulkanDevice::ShutdownFrameSyncObjects()
{
    for (FrameSync& frameSync : m_FrameSyncObjects)
    {
        if (frameSync.m_ImageAvailable != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, frameSync.m_ImageAvailable, nullptr);
            frameSync.m_ImageAvailable = VK_NULL_HANDLE;
        }

        if (frameSync.m_RenderFinished != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Device, frameSync.m_RenderFinished, nullptr);
            frameSync.m_RenderFinished = VK_NULL_HANDLE;
        }

        if (frameSync.m_InFlightFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(m_Device, frameSync.m_InFlightFence, nullptr);
            frameSync.m_InFlightFence = VK_NULL_HANDLE;
        }
    }
}

void VulkanDevice::InitializePresentationObjects(const NativeWindowHandle& nativeWindowHandle)
{
    if (m_HasPresentationObjects)
    {
        RTRLAB_ASSERT_MSG(m_NativeWindowHandle.m_System == nativeWindowHandle.m_System &&
                              m_NativeWindowHandle.m_Window == nativeWindowHandle.m_Window &&
                              m_NativeWindowHandle.m_Display == nativeWindowHandle.m_Display &&
                              m_NativeWindowHandle.m_Layer == nativeWindowHandle.m_Layer,
                          "VulkanDevice currently supports a single presentation target per device.");
        return;
    }

    InitializeInstance();
    m_Surface = CreateSurface(m_Instance, nativeWindowHandle);
    InitializeDeviceObjectsForSurface(m_Surface);
    InitializeFrameSyncObjects();
    m_NativeWindowHandle = nativeWindowHandle;
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_HasPresentationObjects = true;
}

void VulkanDevice::ShutdownPresentationObjects()
{
    if (!m_HasPresentationObjects)
        return;

    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    ShutdownFrameSyncObjects();

    if (m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    m_PresentQueue = m_GraphicsQueue;
    m_PresentQueueFamily = m_GraphicsQueueFamily;
    m_NativeWindowHandle = {};
    m_HasPresentationObjects = false;
}

void VulkanDevice::ShutdownDeviceObjects()
{
    if (!m_HasDeviceObjects)
        return;

    if (m_Device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_Device);

    m_CommandList.Shutdown();
    ShutdownFrameUploadArenas();

    ShutdownAllocator();

    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    m_PhysicalDevice = VK_NULL_HANDLE;
    m_GraphicsQueue = VK_NULL_HANDLE;
    m_PresentQueue = VK_NULL_HANDLE;
    m_GraphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    m_PresentQueueFamily = std::numeric_limits<uint32_t>::max();
    m_CurrentFrameSlot = 0;
    m_FrameInProgress = false;
    m_FrameSubmitted = false;
    m_MinUniformBufferOffsetAlignment = 1;
    m_HasDeviceObjects = false;
}
