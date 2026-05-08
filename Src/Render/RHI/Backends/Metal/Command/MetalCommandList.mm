#include "Render/RHI/Backends/Metal/Command/MetalCommandList.h"

#include <algorithm>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Metal/Resources/MetalBuffer.h"
#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/Backends/Metal/Device/MetalDevice.h"
#include "Render/RHI/Backends/Metal/Pipeline/MetalGraphicsPipeline.h"
#include "Render/RHI/Backends/Metal/Pipeline/MetalPipelineLayout.h"
#include "Render/RHI/Backends/Metal/Resources/MetalResourceSet.h"
#include "Render/RHI/Backends/Metal/Pipeline/MetalShaderProgram.h"
#include "Render/RHI/Backends/Metal/Resources/MetalTexture.h"

using namespace MetalRHI;

MetalCommandList::MetalCommandList() : m_Data(new MetalCommandListData()) {}

MetalCommandList::~MetalCommandList()
{
    Shutdown();
    delete m_Data;
    m_Data = nullptr;
}

void MetalCommandList::Initialize(MetalDeviceData* deviceData)
{
    Shutdown();
    m_Data->m_DeviceData = deviceData;
}

void MetalCommandList::Shutdown()
{
    if (m_Data == nullptr)
        return;

    if (m_Data->m_RenderEncoder != nil)
    {
        [m_Data->m_RenderEncoder endEncoding];
        [m_Data->m_RenderEncoder release];
        m_Data->m_RenderEncoder = nil;
    }

    m_Data->m_DeviceData = nullptr;
}

void MetalCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_DeviceData != nullptr,
                      "Metal command list must be initialized before rendering.");
    RTRLAB_ASSERT_MSG(m_Data->m_DeviceData->m_CurrentCommandBuffer != nil,
                      "Metal rendering requires an active command buffer.");

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    for (uint32_t colorIndex = 0; colorIndex < static_cast<uint32_t>(renderingInfo.m_ColorAttachments.size());
         ++colorIndex)
    {
        const ColorAttachmentInfo& colorAttachmentInfo = renderingInfo.m_ColorAttachments[colorIndex];
        RTRLAB_ASSERT_MSG(colorAttachmentInfo.m_View != nullptr,
                          "Metal BeginRendering requires non-null color attachment views.");
        Texture* colorTexture = colorAttachmentInfo.m_View->GetTexture();
        RTRLAB_ASSERT_MSG(colorTexture != nullptr, "Metal color attachment views must reference textures.");
        RTRLAB_ASSERT_MSG(!IsDepthFormat(colorTexture->GetDesc().m_Format),
                          "Metal color attachments must use color formats.");

        MTLRenderPassColorAttachmentDescriptor* attachment = renderPassDescriptor.colorAttachments[colorIndex];
        attachment.texture = GetMetalTextureFromView(colorAttachmentInfo.m_View);
        attachment.loadAction = ToMetalLoadAction(colorAttachmentInfo.m_LoadOp);
        attachment.storeAction = ToMetalStoreAction(colorAttachmentInfo.m_StoreOp);
        attachment.clearColor = MTLClearColorMake(colorAttachmentInfo.m_ClearValue.m_R,
                                                  colorAttachmentInfo.m_ClearValue.m_G,
                                                  colorAttachmentInfo.m_ClearValue.m_B,
                                                  colorAttachmentInfo.m_ClearValue.m_A);
    }

    if (renderingInfo.m_DepthAttachment.m_View != nullptr)
    {
        Texture* depthTexture = renderingInfo.m_DepthAttachment.m_View->GetTexture();
        RTRLAB_ASSERT_MSG(depthTexture != nullptr, "Metal depth attachment views must reference textures.");
        RTRLAB_ASSERT_MSG(IsDepthFormat(depthTexture->GetDesc().m_Format),
                          "Metal depth attachments must use depth/stencil formats.");

        id<MTLTexture> depthStencilTexture = GetMetalTextureFromView(renderingInfo.m_DepthAttachment.m_View);
        renderPassDescriptor.depthAttachment.texture = depthStencilTexture;
        renderPassDescriptor.depthAttachment.loadAction = ToMetalLoadAction(renderingInfo.m_DepthAttachment.m_LoadOp);
        renderPassDescriptor.depthAttachment.storeAction =
            ToMetalStoreAction(renderingInfo.m_DepthAttachment.m_StoreOp);
        renderPassDescriptor.depthAttachment.clearDepth = renderingInfo.m_DepthAttachment.m_ClearValue.m_Depth;

        if (HasStencilComponent(depthTexture->GetDesc().m_Format))
        {
            renderPassDescriptor.stencilAttachment.texture = depthStencilTexture;
            renderPassDescriptor.stencilAttachment.loadAction =
                ToMetalLoadAction(renderingInfo.m_DepthAttachment.m_LoadOp);
            renderPassDescriptor.stencilAttachment.storeAction =
                ToMetalStoreAction(renderingInfo.m_DepthAttachment.m_StoreOp);
            renderPassDescriptor.stencilAttachment.clearStencil =
                renderingInfo.m_DepthAttachment.m_ClearValue.m_Stencil;
        }
    }

    m_Data->m_RenderEncoder =
        [[m_Data->m_DeviceData->m_CurrentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor] retain];
    RTRLAB_ASSERT_MSG(m_Data->m_RenderEncoder != nil, "Failed to create the Metal render command encoder.");
}

void MetalCommandList::EndRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Metal EndRendering requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal EndRendering requires an active render encoder.");

    [m_Data->m_RenderEncoder endEncoding];
    [m_Data->m_RenderEncoder release];
    m_Data->m_RenderEncoder = nil;

    ShellCommandListBase::EndRendering();
}

void MetalCommandList::BindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    ShellCommandListBase::BindGraphicsPipeline(pipeline);

    if (pipeline == nullptr)
        return;

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal graphics pipelines require an active render encoder.");
    const MetalGraphicsPipeline& metalPipeline = GetMetalGraphicsPipeline(pipeline);
    [m_Data->m_RenderEncoder setRenderPipelineState:metalPipeline.GetPipelineState()];
    [m_Data->m_RenderEncoder setDepthStencilState:metalPipeline.GetDepthStencilState()];
    [m_Data->m_RenderEncoder setCullMode:ToMetalCullMode(metalPipeline.GetDesc().m_RasterState.m_CullMode)];
    [m_Data->m_RenderEncoder setFrontFacingWinding:ToMetalWinding(metalPipeline.GetDesc().m_RasterState.m_FrontFace)];
    [m_Data->m_RenderEncoder
        setTriangleFillMode:ToMetalTriangleFillMode(metalPipeline.GetDesc().m_RasterState.m_FillMode)];

    for (const auto& [setIndex, resourceSet] : m_ResourceSets)
    {
        if (resourceSet != nullptr)
            BindResourceSet(setIndex, resourceSet);
    }

    if (!m_MeshBinding.m_VertexBuffers.empty())
    {
        BindVertexBuffers(0,
                          m_MeshBinding.m_VertexBuffers.data(),
                          static_cast<uint32_t>(m_MeshBinding.m_VertexBuffers.size()),
                          m_VertexOffsets.empty() ? nullptr : m_VertexOffsets.data());
    }
}

void MetalCommandList::BindComputePipeline(ComputePipeline*)
{
    RTRLAB_ASSERT_MSG(false,
                      "Metal compute pipelines are not implemented yet. This backend no longer falls back to the "
                      "shell compute path.");
}

void MetalCommandList::BindResourceSet(uint32_t setIndex, ResourceSet* resourceSet)
{
    ShellCommandListBase::BindResourceSet(setIndex, resourceSet);

    if (resourceSet == nullptr || m_GraphicsPipeline == nullptr)
        return;

    RTRLAB_ASSERTF(resourceSet->GetSetIndex() == setIndex,
                   "Metal BindResourceSet expected resource set {} but received set {}.",
                   setIndex,
                   resourceSet->GetSetIndex());
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal resource sets require an active render encoder.");

    const MetalGraphicsPipeline& metalPipeline = GetMetalGraphicsPipeline(m_GraphicsPipeline);
    PipelineLayout* pipelineLayout = metalPipeline.GetDesc().m_PipelineLayout;
    RTRLAB_ASSERT_MSG(pipelineLayout != nullptr, "Metal BindResourceSet requires a valid PipelineLayout.");
    RTRLAB_ASSERT_MSG(resourceSet->GetLayout() == pipelineLayout,
                      "Metal BindResourceSet currently requires resource sets created from the bound pipeline layout.");

    MetalResourceSet& metalResourceSet = GetMetalResourceSet(resourceSet);
    const MetalShaderProgram& shaderProgram = GetMetalShaderProgram(metalPipeline.GetDesc().m_ShaderProgram);
    const MetalStageBindingPlan* vertexStagePlan = shaderProgram.FindStageBindingPlan(setIndex, ShaderStage::Vertex);
    if (vertexStagePlan != nullptr)
    {
        const MetalStageArgumentEncoderEntry* encoderEntry =
            metalPipeline.FindArgumentEncoderEntry(setIndex, ShaderStage::Vertex);
        RTRLAB_ASSERT_MSG(encoderEntry != nullptr && encoderEntry->m_Encoder != nil,
                          "Metal vertex-stage argument-buffer binding requires a valid encoder.");
        id<MTLBuffer> argumentBuffer =
            metalResourceSet.GetEncodedArgumentBuffer(*vertexStagePlan, encoderEntry->m_Encoder);
        [m_Data->m_RenderEncoder setVertexBuffer:argumentBuffer offset:0 atIndex:encoderEntry->m_Slot];
    }

    const MetalStageBindingPlan* fragmentStagePlan =
        shaderProgram.FindStageBindingPlan(setIndex, ShaderStage::Fragment);
    if (fragmentStagePlan != nullptr)
    {
        const MetalStageArgumentEncoderEntry* encoderEntry =
            metalPipeline.FindArgumentEncoderEntry(setIndex, ShaderStage::Fragment);
        RTRLAB_ASSERT_MSG(encoderEntry != nullptr && encoderEntry->m_Encoder != nil,
                          "Metal fragment-stage argument-buffer binding requires a valid encoder.");
        id<MTLBuffer> argumentBuffer =
            metalResourceSet.GetEncodedArgumentBuffer(*fragmentStagePlan, encoderEntry->m_Encoder);
        [m_Data->m_RenderEncoder setFragmentBuffer:argumentBuffer offset:0 atIndex:encoderEntry->m_Slot];
    }
}

void MetalCommandList::PushConstants(ShaderStage, uint32_t, uint32_t, const void*)
{
    RTRLAB_ASSERT_MSG(false,
                      "Metal push constants are not implemented yet. This backend no longer records shell-only "
                      "push-constant state.");
}

void MetalCommandList::BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets)
{
    ShellCommandListBase::BindMesh(meshBinding, vertexOffsets);

    if (!meshBinding.m_VertexBuffers.empty())
    {
        BindVertexBuffers(0,
                          meshBinding.m_VertexBuffers.data(),
                          static_cast<uint32_t>(meshBinding.m_VertexBuffers.size()),
                          vertexOffsets);
    }

    if (meshBinding.m_IndexBuffer != nullptr)
        BindIndexBuffer(meshBinding.m_IndexBuffer, 0, meshBinding.m_IndexType);
}

void MetalCommandList::BindVertexBuffers(uint32_t firstSlot,
                                         Buffer* const* buffers,
                                         uint32_t count,
                                         const uint64_t* offsets)
{
    ShellCommandListBase::BindVertexBuffers(firstSlot, buffers, count, offsets);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal vertex buffers require an active render encoder.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Metal vertex buffers require a bound graphics pipeline.");
    const MetalGraphicsPipeline& metalPipeline = GetMetalGraphicsPipeline(m_GraphicsPipeline);
    const uint32_t slotBase = metalPipeline.GetVertexBufferSlotBase();

    for (uint32_t index = 0; index < count; ++index)
    {
        RTRLAB_ASSERT_MSG(buffers[index] != nullptr, "Metal BindVertexBuffers requires non-null buffers.");
        const uint64_t offset = offsets != nullptr ? offsets[index] : 0;
        [m_Data->m_RenderEncoder setVertexBuffer:GetMetalBuffer(buffers[index]).GetMetalBuffer()
                                          offset:static_cast<NSUInteger>(offset)
                                         atIndex:slotBase + firstSlot + index];
    }
}

void MetalCommandList::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
{
    ShellCommandListBase::BindIndexBuffer(buffer, offset, indexType);
}

void MetalCommandList::SetViewport(float x, float y, float w, float h, float zmin, float zmax)
{
    ShellCommandListBase::SetViewport(x, y, w, h, zmin, zmax);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal SetViewport requires an active render encoder.");

    MTLViewport viewport;
    viewport.originX = x;
    viewport.originY = y;
    viewport.width = w;
    viewport.height = h;
    viewport.znear = zmin;
    viewport.zfar = zmax;
    [m_Data->m_RenderEncoder setViewport:viewport];
}

void MetalCommandList::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    ShellCommandListBase::SetScissor(x, y, w, h);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal SetScissor requires an active render encoder.");

    MTLScissorRect scissor;
    scissor.x = static_cast<NSUInteger>(std::max<int32_t>(x, 0));
    scissor.y = static_cast<NSUInteger>(std::max<int32_t>(y, 0));
    scissor.width = static_cast<NSUInteger>(w);
    scissor.height = static_cast<NSUInteger>(h);
    [m_Data->m_RenderEncoder setScissorRect:scissor];
}

void MetalCommandList::Dispatch(uint32_t, uint32_t, uint32_t)
{
    RTRLAB_ASSERT_MSG(false,
                      "Metal dispatch is not implemented yet. This backend no longer falls back to the shell "
                      "compute path.");
}

void MetalCommandList::TextureBarrier(
    Texture* texture, TextureState oldState, TextureState newState, ShaderStage srcStage, ShaderStage dstStage)
{
    (void)srcStage;
    (void)dstStage;

    if (texture == nullptr || oldState == newState)
        return;

    // Metal has no Vulkan-style image layout transitions in this v1 path. The
    // ordering between blit/render encoders inside the command buffer carries
    // the current resource-usage dependency.
}

void MetalCommandList::BufferBarrier(
    Buffer* buffer, BufferState oldState, BufferState newState, ShaderStage srcStage, ShaderStage dstStage)
{
    (void)srcStage;
    (void)dstStage;

    if (buffer == nullptr || oldState == newState)
        return;

    // See TextureBarrier: current Metal resource transitions are represented by
    // encoder/command-buffer ordering rather than an explicit barrier command.
}

void MetalCommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
{
    ShellCommandListBase::DrawIndexed(indexCount, firstIndex, vertexOffset);

    RTRLAB_ASSERT_MSG(m_IsRendering, "Metal DrawIndexed requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal DrawIndexed requires an active render encoder.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Metal DrawIndexed requires a bound graphics pipeline.");
    RTRLAB_ASSERT_MSG(m_IndexBuffer != nullptr, "Metal DrawIndexed requires a bound index buffer.");

    const MetalGraphicsPipeline& metalPipeline = GetMetalGraphicsPipeline(m_GraphicsPipeline);
    const NSUInteger indexSize = m_IndexType == IndexType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    const NSUInteger indexBufferOffset =
        static_cast<NSUInteger>(m_IndexOffset + static_cast<uint64_t>(firstIndex) * indexSize);

    [m_Data->m_RenderEncoder drawIndexedPrimitives:ToMetalPrimitiveType(metalPipeline.GetDesc().m_Topology)
                                        indexCount:indexCount
                                         indexType:ToMetalIndexType(m_IndexType)
                                       indexBuffer:GetMetalBuffer(m_IndexBuffer).GetMetalBuffer()
                                 indexBufferOffset:indexBufferOffset
                                     instanceCount:1
                                        baseVertex:vertexOffset
                                      baseInstance:0];
}

void MetalCommandList::CopyBuffer(Buffer* sourceBuffer,
                                  Buffer* destinationBuffer,
                                  std::span<const BufferCopyRegion> regions)
{
    RTRLAB_ASSERT_MSG(!m_IsRendering, "Metal CopyBuffer cannot be recorded inside an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_DeviceData != nullptr &&
                          m_Data->m_DeviceData->m_CurrentCommandBuffer != nil,
                      "Metal CopyBuffer requires an active command buffer.");
    RTRLAB_ASSERT_MSG(sourceBuffer != nullptr && destinationBuffer != nullptr,
                      "Metal CopyBuffer requires valid source and destination buffers.");
    RTRLAB_ASSERT_MSG(!regions.empty(), "Metal CopyBuffer requires at least one copy region.");

    id<MTLBlitCommandEncoder> blitEncoder = [m_Data->m_DeviceData->m_CurrentCommandBuffer blitCommandEncoder];
    RTRLAB_ASSERT_MSG(blitEncoder != nil, "Failed to create the Metal blit command encoder.");

    id<MTLBuffer> sourceMetalBuffer = GetMetalBuffer(sourceBuffer).GetMetalBuffer();
    id<MTLBuffer> destinationMetalBuffer = GetMetalBuffer(destinationBuffer).GetMetalBuffer();
    for (const BufferCopyRegion& region : regions)
    {
        RTRLAB_ASSERT_MSG(region.m_Size > 0, "Metal CopyBuffer regions must have a non-zero size.");
        [blitEncoder copyFromBuffer:sourceMetalBuffer
                       sourceOffset:region.m_SourceOffset
                           toBuffer:destinationMetalBuffer
                  destinationOffset:region.m_DestinationOffset
                               size:region.m_Size];
    }

    [blitEncoder endEncoding];
}

void MetalCommandList::CopyBufferToTexture(Buffer* sourceBuffer,
                                           Texture* destinationTexture,
                                           std::span<const BufferTextureCopyRegion> regions)
{
    RTRLAB_ASSERT_MSG(!m_IsRendering, "Metal CopyBufferToTexture cannot be recorded inside an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_DeviceData != nullptr &&
                          m_Data->m_DeviceData->m_CurrentCommandBuffer != nil,
                      "Metal CopyBufferToTexture requires an active command buffer.");
    RTRLAB_ASSERT_MSG(sourceBuffer != nullptr && destinationTexture != nullptr,
                      "Metal CopyBufferToTexture requires a valid source buffer and destination texture.");
    RTRLAB_ASSERT_MSG(!regions.empty(), "Metal CopyBufferToTexture requires at least one copy region.");

    id<MTLBlitCommandEncoder> blitEncoder = [m_Data->m_DeviceData->m_CurrentCommandBuffer blitCommandEncoder];
    RTRLAB_ASSERT_MSG(blitEncoder != nil, "Failed to create the Metal blit command encoder.");

    id<MTLBuffer> sourceMetalBuffer = GetMetalBuffer(sourceBuffer).GetMetalBuffer();
    id<MTLTexture> destinationMetalTexture = GetMetalTextureFromTexture(destinationTexture);
    const TextureDesc& destinationDesc = destinationTexture->GetDesc();
    const uint32_t bytesPerPixel = GetFormatBytesPerPixel(destinationDesc.m_Format);

    for (const BufferTextureCopyRegion& region : regions)
    {
        RTRLAB_ASSERT_MSG(region.m_LayerCount > 0, "Metal CopyBufferToTexture regions must target at least one layer.");
        RTRLAB_ASSERT_MSG(region.m_TextureExtent.m_Width > 0 && region.m_TextureExtent.m_Height > 0 &&
                              region.m_TextureExtent.m_Depth > 0,
                          "Metal CopyBufferToTexture regions must have a non-zero extent.");

        const NSUInteger sourceBytesPerRow = static_cast<NSUInteger>(
            region.m_BufferRowPitch != 0 ? region.m_BufferRowPitch : region.m_TextureExtent.m_Width * bytesPerPixel);
        const NSUInteger sourceBytesPerImage = static_cast<NSUInteger>(
            region.m_BufferRowsPerImage != 0 ? sourceBytesPerRow * region.m_BufferRowsPerImage
                                             : sourceBytesPerRow * region.m_TextureExtent.m_Height);

        if (destinationDesc.m_Type == TextureType::Tex3D)
        {
            RTRLAB_ASSERT_MSG(region.m_BaseArrayLayer == 0 && region.m_LayerCount == 1,
                              "Metal CopyBufferToTexture uses destination slices for array textures. Tex3D copies "
                              "must target exactly one destination slice.");
            [blitEncoder copyFromBuffer:sourceMetalBuffer
                           sourceOffset:region.m_BufferOffset
                      sourceBytesPerRow:sourceBytesPerRow
                    sourceBytesPerImage:sourceBytesPerImage
                             sourceSize:MTLSizeMake(region.m_TextureExtent.m_Width,
                                                    region.m_TextureExtent.m_Height,
                                                    region.m_TextureExtent.m_Depth)
                              toTexture:destinationMetalTexture
                       destinationSlice:0
                       destinationLevel:region.m_MipLevel
                      destinationOrigin:MTLOriginMake(region.m_TextureOffset.m_X,
                                                      region.m_TextureOffset.m_Y,
                                                      region.m_TextureOffset.m_Z)];
            continue;
        }

        for (uint32_t layerIndex = 0; layerIndex < region.m_LayerCount; ++layerIndex)
        {
            [blitEncoder copyFromBuffer:sourceMetalBuffer
                           sourceOffset:region.m_BufferOffset + static_cast<uint64_t>(layerIndex) * sourceBytesPerImage
                      sourceBytesPerRow:sourceBytesPerRow
                    sourceBytesPerImage:sourceBytesPerImage
                             sourceSize:MTLSizeMake(region.m_TextureExtent.m_Width, region.m_TextureExtent.m_Height, 1)
                              toTexture:destinationMetalTexture
                       destinationSlice:region.m_BaseArrayLayer + layerIndex
                       destinationLevel:region.m_MipLevel
                      destinationOrigin:MTLOriginMake(region.m_TextureOffset.m_X, region.m_TextureOffset.m_Y, 0)];
        }
    }

    [blitEncoder endEncoding];
}
