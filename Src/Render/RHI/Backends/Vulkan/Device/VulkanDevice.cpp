#include "Render/RHI/Backends/Vulkan/Device/VulkanDevice.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "Render/RHI/Backends/Vulkan/Resources/VulkanBuffer.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanDescriptors.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanGraphicsPipeline.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanPipelineLayout.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanResourceSet.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanSampler.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanShaderProgram.h"
#include "Render/RHI/Backends/Vulkan/Presentation/VulkanSurface.h"
#include "Render/RHI/Backends/Vulkan/Presentation/VulkanSwapchain.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanTexture.h"

using namespace VulkanRHI;

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

    m_CommandList.ResetState();

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
