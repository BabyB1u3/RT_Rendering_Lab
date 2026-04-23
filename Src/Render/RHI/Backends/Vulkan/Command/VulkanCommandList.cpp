#include "Render/RHI/Backends/Vulkan/Command/VulkanCommandList.h"

#include <algorithm>
#include <vector>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanBarriers.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanBuffer.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanConversions.h"
#include "Render/RHI/Backends/Vulkan/Device/VulkanDevice.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanGraphicsPipeline.h"
#include "Render/RHI/Backends/Vulkan/Pipeline/VulkanPipelineLayout.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanResourceSet.h"
#include "Render/RHI/Backends/Vulkan/Resources/VulkanTexture.h"

using namespace VulkanRHI;

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

    uint32_t layerCount = 1;
    bool hasResolvedLayerCount = false;
    auto updateLayerCount = [&layerCount, &hasResolvedLayerCount](const TextureView* view)
    {
        const uint32_t viewLayerCount = std::max(view->GetDesc().m_ArrayLayerCount, 1u);
        if (!hasResolvedLayerCount)
        {
            layerCount = viewLayerCount;
            hasResolvedLayerCount = true;
            return;
        }

        RTRLAB_ASSERT_MSG(layerCount == viewLayerCount,
                          "Vulkan BeginRendering requires every attachment view to target the same layer count.");
    };

    std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos(renderingInfo.m_ColorAttachments.size());
    for (size_t attachmentIndex = 0; attachmentIndex < renderingInfo.m_ColorAttachments.size(); ++attachmentIndex)
    {
        const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments[attachmentIndex];
        RTRLAB_ASSERT_MSG(colorAttachment.m_View != nullptr,
                          "Vulkan BeginRendering requires non-null color attachment views.");

        TextureView* colorView = colorAttachment.m_View;
        Texture* colorTexture = colorView->GetTexture();
        RTRLAB_ASSERT_MSG(colorTexture != nullptr,
                          "Vulkan BeginRendering color attachment views must reference textures.");
        RTRLAB_ASSERT_MSG(!IsDepthFormat(colorTexture->GetDesc().m_Format),
                          "Vulkan BeginRendering color attachments must use color formats.");
        RTRLAB_ASSERT_MSG(GetTrackedTextureState(colorTexture) == TextureState::RenderTarget,
                          "Vulkan BeginRendering requires every color attachment to already be transitioned to "
                          "RenderTarget.");
        RTRLAB_ASSERT_MSG(GetTrackedImageLayout(colorTexture) == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          "Vulkan BeginRendering requires every color attachment to already be in "
                          "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.");
        updateLayerCount(colorView);

        VkRenderingAttachmentInfo& colorAttachmentInfo = colorAttachmentInfos[attachmentIndex];
        colorAttachmentInfo = MakeVkStruct<VkRenderingAttachmentInfo, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO>();
        colorAttachmentInfo.imageView = GetVkImageViewFromTextureView(colorView);
        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.loadOp = ToVkAttachmentLoadOp(colorAttachment.m_LoadOp);
        colorAttachmentInfo.storeOp = ToVkAttachmentStoreOp(colorAttachment.m_StoreOp);
        colorAttachmentInfo.clearValue.color = {{colorAttachment.m_ClearValue.m_R,
                                                 colorAttachment.m_ClearValue.m_G,
                                                 colorAttachment.m_ClearValue.m_B,
                                                 colorAttachment.m_ClearValue.m_A}};
    }

    VkRenderingAttachmentInfo depthAttachmentInfo{};
    VkRenderingAttachmentInfo stencilAttachmentInfo{};
    VkRenderingAttachmentInfo* depthAttachmentPtr = nullptr;
    VkRenderingAttachmentInfo* stencilAttachmentPtr = nullptr;
    if (renderingInfo.m_DepthAttachment.m_View != nullptr)
    {
        TextureView* depthView = renderingInfo.m_DepthAttachment.m_View;
        Texture* depthTexture = depthView->GetTexture();
        RTRLAB_ASSERT_MSG(depthTexture != nullptr,
                          "Vulkan BeginRendering depth attachment views must reference textures.");
        RTRLAB_ASSERT_MSG(IsDepthFormat(depthTexture->GetDesc().m_Format),
                          "Vulkan BeginRendering depth attachments must use depth/stencil formats.");
        RTRLAB_ASSERT_MSG(GetTrackedTextureState(depthTexture) == TextureState::DepthStencil,
                          "Vulkan BeginRendering requires the depth attachment to already be transitioned to "
                          "DepthStencil.");
        RTRLAB_ASSERT_MSG(GetTrackedImageLayout(depthTexture) ==
                              ToVkImageLayout(TextureState::DepthStencil, depthTexture->GetDesc()),
                          "Vulkan BeginRendering requires the depth attachment to already be in its "
                          "depth/stencil attachment layout.");
        updateLayerCount(depthView);

        depthAttachmentInfo = MakeVkStruct<VkRenderingAttachmentInfo, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO>();
        depthAttachmentInfo.imageView = GetVkImageViewFromTextureView(depthView);
        depthAttachmentInfo.imageLayout = ToVkImageLayout(TextureState::DepthStencil, depthTexture->GetDesc());
        depthAttachmentInfo.loadOp = ToVkAttachmentLoadOp(renderingInfo.m_DepthAttachment.m_LoadOp);
        depthAttachmentInfo.storeOp = ToVkAttachmentStoreOp(renderingInfo.m_DepthAttachment.m_StoreOp);
        depthAttachmentInfo.clearValue.depthStencil = {renderingInfo.m_DepthAttachment.m_ClearValue.m_Depth,
                                                       renderingInfo.m_DepthAttachment.m_ClearValue.m_Stencil};
        depthAttachmentPtr = &depthAttachmentInfo;

        if (HasStencilComponent(depthTexture->GetDesc().m_Format))
        {
            stencilAttachmentInfo = depthAttachmentInfo;
            stencilAttachmentPtr = &stencilAttachmentInfo;
        }
    }

    VkRenderingInfo vkRenderingInfo = MakeVkStruct<VkRenderingInfo, VK_STRUCTURE_TYPE_RENDERING_INFO>();
    vkRenderingInfo.renderArea.offset = {renderingInfo.m_RenderArea.m_X, renderingInfo.m_RenderArea.m_Y};
    vkRenderingInfo.renderArea.extent = {renderingInfo.m_RenderArea.m_Width, renderingInfo.m_RenderArea.m_Height};
    vkRenderingInfo.layerCount = layerCount;
    vkRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentInfos.size());
    vkRenderingInfo.pColorAttachments = colorAttachmentInfos.empty() ? nullptr : colorAttachmentInfos.data();
    vkRenderingInfo.pDepthAttachment = depthAttachmentPtr;
    vkRenderingInfo.pStencilAttachment = stencilAttachmentPtr;

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

    vkCmdEndRendering(m_CommandBuffer);
    for (const ColorAttachmentInfo& colorAttachment : m_RenderingInfo.m_ColorAttachments)
    {
        Texture* colorTexture = colorAttachment.m_View != nullptr ? colorAttachment.m_View->GetTexture() : nullptr;
        RTRLAB_ASSERT_MSG(colorTexture != nullptr, "Vulkan EndRendering requires valid color attachment textures.");
        RTRLAB_ASSERT_MSG(GetTrackedTextureState(colorTexture) == TextureState::RenderTarget,
                          "Vulkan EndRendering expects color attachments to remain in RenderTarget state until an "
                          "explicit barrier transitions them elsewhere.");
    }

    if (m_RenderingInfo.m_DepthAttachment.m_View != nullptr)
    {
        Texture* depthTexture = m_RenderingInfo.m_DepthAttachment.m_View->GetTexture();
        RTRLAB_ASSERT_MSG(depthTexture != nullptr, "Vulkan EndRendering requires a valid depth attachment texture.");
        RTRLAB_ASSERT_MSG(GetTrackedTextureState(depthTexture) == TextureState::DepthStencil,
                          "Vulkan EndRendering expects the depth attachment to remain in DepthStencil state until an "
                          "explicit barrier transitions it elsewhere.");
    }

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

void VulkanCommandList::CopyBuffer(Buffer* sourceBuffer,
                                   Buffer* destinationBuffer,
                                   std::span<const BufferCopyRegion> regions)
{
    RTRLAB_ASSERT_MSG(!m_IsRendering, "Vulkan CopyBuffer cannot be recorded inside an active rendering scope.");
    RTRLAB_ASSERT_MSG(sourceBuffer != nullptr && destinationBuffer != nullptr,
                      "Vulkan CopyBuffer requires valid source and destination buffers.");
    RTRLAB_ASSERT_MSG(!regions.empty(), "Vulkan CopyBuffer requires at least one copy region.");

    VulkanBuffer& sourceVulkanBuffer = GetVulkanBuffer(sourceBuffer);
    VulkanBuffer& destinationVulkanBuffer = GetVulkanBuffer(destinationBuffer);
    RTRLAB_ASSERT_MSG(sourceVulkanBuffer.GetCurrentState() == BufferState::CopySource,
                      "Vulkan CopyBuffer requires the source buffer to be in CopySource state.");
    RTRLAB_ASSERT_MSG(destinationVulkanBuffer.GetCurrentState() == BufferState::CopyDest,
                      "Vulkan CopyBuffer requires the destination buffer to be in CopyDest state.");

    std::vector<VkBufferCopy> vkRegions(regions.size());
    for (size_t regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
    {
        const BufferCopyRegion& region = regions[regionIndex];
        RTRLAB_ASSERT_MSG(region.m_Size > 0, "Vulkan CopyBuffer regions must have a non-zero size.");
        vkRegions[regionIndex].srcOffset = region.m_SourceOffset;
        vkRegions[regionIndex].dstOffset = region.m_DestinationOffset;
        vkRegions[regionIndex].size = region.m_Size;
    }

    vkCmdCopyBuffer(m_CommandBuffer,
                    sourceVulkanBuffer.GetVkBuffer(),
                    destinationVulkanBuffer.GetVkBuffer(),
                    static_cast<uint32_t>(vkRegions.size()),
                    vkRegions.data());
}

void VulkanCommandList::CopyBufferToTexture(Buffer* sourceBuffer,
                                            Texture* destinationTexture,
                                            std::span<const BufferTextureCopyRegion> regions)
{
    RTRLAB_ASSERT_MSG(!m_IsRendering,
                      "Vulkan CopyBufferToTexture cannot be recorded inside an active rendering scope.");
    RTRLAB_ASSERT_MSG(sourceBuffer != nullptr && destinationTexture != nullptr,
                      "Vulkan CopyBufferToTexture requires a valid source buffer and destination texture.");
    RTRLAB_ASSERT_MSG(!regions.empty(), "Vulkan CopyBufferToTexture requires at least one copy region.");

    VulkanBuffer& sourceVulkanBuffer = GetVulkanBuffer(sourceBuffer);
    RTRLAB_ASSERT_MSG(sourceVulkanBuffer.GetCurrentState() == BufferState::CopySource,
                      "Vulkan CopyBufferToTexture requires the source buffer to be in CopySource state.");
    RTRLAB_ASSERT_MSG(GetTrackedTextureState(destinationTexture) == TextureState::CopyDest,
                      "Vulkan CopyBufferToTexture requires the destination texture to be in CopyDest state.");

    const TextureDesc& destinationDesc = destinationTexture->GetDesc();
    const VkImageLayout destinationLayout = ToVkImageLayout(TextureState::CopyDest, destinationDesc);
    RTRLAB_ASSERT_MSG(GetTrackedImageLayout(destinationTexture) == destinationLayout,
                      "Vulkan CopyBufferToTexture requires the destination texture to already be in "
                      "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.");

    const uint32_t bytesPerPixel = GetFormatBytesPerPixel(destinationDesc.m_Format);
    std::vector<VkBufferImageCopy> vkRegions(regions.size());
    for (size_t regionIndex = 0; regionIndex < regions.size(); ++regionIndex)
    {
        const BufferTextureCopyRegion& region = regions[regionIndex];
        RTRLAB_ASSERT_MSG(region.m_LayerCount > 0,
                          "Vulkan CopyBufferToTexture regions must target at least one layer.");
        RTRLAB_ASSERT_MSG(region.m_TextureExtent.m_Width > 0 && region.m_TextureExtent.m_Height > 0 &&
                              region.m_TextureExtent.m_Depth > 0,
                          "Vulkan CopyBufferToTexture regions must have a non-zero extent.");
        RTRLAB_ASSERT_MSG(region.m_BufferRowPitch == 0 || (region.m_BufferRowPitch % bytesPerPixel) == 0,
                          "Vulkan CopyBufferToTexture row pitch must be a multiple of the destination texel size.");

        VkBufferImageCopy& vkRegion = vkRegions[regionIndex];
        vkRegion.bufferOffset = region.m_BufferOffset;
        vkRegion.bufferRowLength = region.m_BufferRowPitch == 0 ? 0 : (region.m_BufferRowPitch / bytesPerPixel);
        vkRegion.bufferImageHeight = region.m_BufferRowsPerImage;
        vkRegion.imageSubresource.aspectMask = ToVkImageAspect(region.m_TextureAspect, destinationDesc.m_Format);
        vkRegion.imageSubresource.mipLevel = region.m_MipLevel;
        vkRegion.imageSubresource.baseArrayLayer = region.m_BaseArrayLayer;
        vkRegion.imageSubresource.layerCount = region.m_LayerCount;
        vkRegion.imageOffset = {static_cast<int32_t>(region.m_TextureOffset.m_X),
                                static_cast<int32_t>(region.m_TextureOffset.m_Y),
                                static_cast<int32_t>(region.m_TextureOffset.m_Z)};
        vkRegion.imageExtent = {
            region.m_TextureExtent.m_Width, region.m_TextureExtent.m_Height, region.m_TextureExtent.m_Depth};
    }

    vkCmdCopyBufferToImage(m_CommandBuffer,
                           sourceVulkanBuffer.GetVkBuffer(),
                           GetVkImageForBarrier(destinationTexture),
                           destinationLayout,
                           static_cast<uint32_t>(vkRegions.size()),
                           vkRegions.data());
}
