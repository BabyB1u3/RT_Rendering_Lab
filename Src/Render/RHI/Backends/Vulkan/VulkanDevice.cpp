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
        RTRLAB_ASSERT_MSG(m_Layout != nullptr, "Vulkan ResourceSet creation requires a valid PipelineLayout.");
        if (const BindingInfo* constantBindingInfo =
                RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
            constantBindingInfo != nullptr && constantBindingInfo->m_ByteSize > 0)
        {
            m_Constants.Resize(constantBindingInfo->m_ByteSize);
        }
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

        const BindingInfo& bindingInfo = ValidateConstantBindingExists();
        RTRLAB_ASSERT_MSG(offset + size <= bindingInfo.m_ByteSize,
                          "Vulkan ResourceSet constant write exceeds the declared UniformBuffer size.");
        m_Constants.SetRaw(offset, data, size);
        ++m_Version;
    }

    void SetBufferArray(uint32_t binding, std::span<const BufferBinding> bufferBindings) override
    {
        const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::StorageBuffer);
        ValidateBindingArrayCount(bindingInfo, bufferBindings.size(), "buffer");
        m_BufferBindings[binding] = std::vector<BufferBinding>(bufferBindings.begin(), bufferBindings.end());
        WriteBufferDescriptor(bindingInfo, m_BufferBindings[binding]);
        ++m_Version;
    }

    void SetTextureArray(uint32_t binding, std::span<const TextureBinding> textureBindings) override
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

    void SetSamplerArray(uint32_t binding, std::span<const SamplerBinding> samplerBindings) override
    {
        const BindingInfo& bindingInfo = RequireBindingInfo(binding, ResourceKind::Sampler);
        ValidateBindingArrayCount(bindingInfo, samplerBindings.size(), "sampler");
        m_SamplerBindings[binding] = std::vector<SamplerBinding>(samplerBindings.begin(), samplerBindings.end());
        WriteSamplerDescriptor(bindingInfo, m_SamplerBindings[binding]);
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

    const BindingInfo& ValidateConstantBindingExists() const
    {
        RTRLAB_ASSERT_MSG(m_Layout != nullptr,
                          "Vulkan ResourceSet constant validation requires a valid PipelineLayout.");
        const BindingInfo* bindingInfo =
            RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        RTRLAB_ASSERTF(bindingInfo != nullptr,
                       "Vulkan ResourceSet set {} has no UniformBuffer binding in its PipelineLayout.",
                       m_SetIndex);
        return *bindingInfo;
    }

    void
    ValidateBindingArrayCount(const BindingInfo& bindingInfo, size_t providedCount, std::string_view resourceKind) const
    {
        RTRLAB_ASSERTF(providedCount == bindingInfo.m_ArrayCount,
                       "Vulkan ResourceSet set {} binding {} expects exactly {} {} descriptor(s), but received {}.",
                       m_SetIndex,
                       bindingInfo.m_Binding,
                       bindingInfo.m_ArrayCount,
                       resourceKind,
                       providedCount);
    }

    const BindingInfo& RequireConstantBindingInfo() const
    {
        const BindingInfo& bindingInfo = ValidateConstantBindingExists();
        RTRLAB_ASSERT_MSG(bindingInfo.m_ArrayCount <= 1,
                          "Vulkan ResourceSet constant uploads currently only support non-array UniformBuffer "
                          "bindings.");
        return bindingInfo;
    }

    void WriteBufferDescriptor(const BindingInfo& bindingInfo, std::span<const BufferBinding> bufferBindings)
    {
        std::vector<VkDescriptorBufferInfo> bufferInfos(bufferBindings.size());
        for (size_t index = 0; index < bufferBindings.size(); ++index)
        {
            const BufferBinding& bufferBinding = bufferBindings[index];
            RTRLAB_ASSERT_MSG(bufferBinding.m_Buffer != nullptr,
                              "Vulkan ResourceSet buffer descriptor writes require valid Buffers.");
            auto* vulkanBuffer = dynamic_cast<VulkanBuffer*>(bufferBinding.m_Buffer);
            RTRLAB_ASSERT_MSG(vulkanBuffer != nullptr, "Buffer is not owned by the Vulkan backend.");

            VkDescriptorBufferInfo& bufferInfo = bufferInfos[index];
            bufferInfo.buffer = vulkanBuffer->GetVkBuffer();
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

    void WriteTextureDescriptor(const BindingInfo& bindingInfo, std::span<const TextureBinding> textureBindings)
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

    void WriteSamplerDescriptor(const BindingInfo& bindingInfo, std::span<const SamplerBinding> samplerBindings)
    {
        std::vector<VkDescriptorImageInfo> imageInfos(samplerBindings.size());
        for (size_t index = 0; index < samplerBindings.size(); ++index)
        {
            const SamplerBinding& samplerBinding = samplerBindings[index];
            RTRLAB_ASSERT_MSG(samplerBinding.m_Sampler != nullptr,
                              "Vulkan ResourceSet sampler descriptor writes require valid Samplers.");
            auto* vulkanSampler = dynamic_cast<VulkanSampler*>(samplerBinding.m_Sampler);
            RTRLAB_ASSERT_MSG(vulkanSampler != nullptr, "Sampler is not owned by the Vulkan backend.");

            VkDescriptorImageInfo& imageInfo = imageInfos[index];
            imageInfo.sampler = vulkanSampler->GetVkSampler();
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

    void ResolveAutoTextureViews(uint32_t binding, std::vector<TextureBinding>& textureBindings)
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

VkAttachmentLoadOp ToVkAttachmentLoadOp(LoadOp loadOp)
{
    switch (loadOp)
    {
        case LoadOp::Clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case LoadOp::Load:
        default:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
}

VkAttachmentStoreOp ToVkAttachmentStoreOp(StoreOp storeOp)
{
    switch (storeOp)
    {
        case StoreOp::DontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        case StoreOp::Store:
        default:
            return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

uint32_t GetFormatBytesPerPixel(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM:
            return 1;
        case Format::RG8_UNORM:
            return 2;
        case Format::RGBA8_UNORM:
        case Format::RGBA8_SRGB:
        case Format::BGRA8_UNORM:
        case Format::BGRA8_SRGB:
        case Format::R32F:
        case Format::R32_UINT:
        case Format::D32_SFLOAT:
            return 4;
        case Format::R16F:
        case Format::D16_UNORM:
            return 2;
        case Format::RG16F:
        case Format::RG32F:
            return 8;
        case Format::RGBA16F:
        case Format::RGBA32F:
            return 16;
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_SFLOAT_S8_UINT:
            return 4;
        default:
            break;
    }

    RTRLAB_ASSERTF(false, "Unsupported Vulkan copy format {}", static_cast<uint32_t>(format));
    return 0;
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

#include "Render/RHI/Backends/Vulkan/VulkanCommandList.inl"
#include "Render/RHI/Backends/Vulkan/VulkanSwapchain.inl"
#include "Render/RHI/Backends/Vulkan/VulkanDeviceImpl.inl"
