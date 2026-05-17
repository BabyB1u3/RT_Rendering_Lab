#include "Render/RHI/RHIUpload.h"

#include <span>

#include "Core/Diagnostics/Assert/Assert.h"

namespace RHIUpload
{
Scope<Buffer> CreateUploadBuffer(Device& device, const void* data, uint64_t size, const char* debugName)
{
    RTRLAB_ASSERT_MSG(data != nullptr, "Upload buffer creation requires non-null source data.");
    RTRLAB_ASSERT_MSG(size > 0, "Upload buffer creation requires a non-zero byte size.");

    BufferDesc uploadDesc;
    uploadDesc.m_Size = size;
    uploadDesc.m_UsageMask = BufferUsage::CopySrc;
    uploadDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    uploadDesc.m_DebugName = debugName;
    Scope<Buffer> uploadBuffer = device.CreateBuffer(uploadDesc);
    device.WriteBuffer(uploadBuffer.get(), 0, data, size);
    return uploadBuffer;
}

void CreateStaticBufferPair(Device& device,
                            const void* data,
                            uint64_t size,
                            BufferUsage targetUsage,
                            const char* targetDebugName,
                            const char* uploadDebugName,
                            Scope<Buffer>& targetBuffer,
                            Scope<Buffer>& uploadBuffer)
{
    RTRLAB_ASSERT_MSG(data != nullptr, "Static buffer creation requires non-null source data.");
    RTRLAB_ASSERT_MSG(size > 0, "Static buffer creation requires a non-zero byte size.");

    BufferDesc targetDesc;
    targetDesc.m_Size = size;
    targetDesc.m_UsageMask = targetUsage | BufferUsage::CopyDst;
    targetDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    targetDesc.m_DebugName = targetDebugName;
    targetBuffer = device.CreateBuffer(targetDesc);

    uploadBuffer = CreateUploadBuffer(device, data, size, uploadDebugName);
}

void UploadStaticBufferPair(CommandList& commandList,
                            ResourceStateTracker& resourceStateTracker,
                            Buffer* uploadBuffer,
                            Buffer* targetBuffer,
                            uint64_t size,
                            BufferState finalState)
{
    RTRLAB_ASSERT_MSG(uploadBuffer != nullptr && targetBuffer != nullptr,
                      "Static buffer upload requires both upload and target buffers.");
    RTRLAB_ASSERT_MSG(size > 0, "Static buffer upload requires a non-zero byte size.");

    resourceStateTracker.Transition(uploadBuffer, BufferState::CopySource);
    resourceStateTracker.Transition(targetBuffer, BufferState::CopyDest);
    resourceStateTracker.FlushBarriers(&commandList);

    const BufferCopyRegion copyRegion{0, 0, size};
    commandList.CopyBuffer(uploadBuffer, targetBuffer, std::span<const BufferCopyRegion>(&copyRegion, 1));

    resourceStateTracker.Transition(targetBuffer, finalState);
    resourceStateTracker.FlushBarriers(&commandList);
}

void UploadFullTexture(CommandList& commandList,
                       ResourceStateTracker& resourceStateTracker,
                       Buffer* uploadBuffer,
                       Texture* targetTexture,
                       uint32_t bytesPerPixel,
                       TextureState finalState)
{
    RTRLAB_ASSERT_MSG(uploadBuffer != nullptr && targetTexture != nullptr,
                      "Texture upload requires both upload buffer and target texture.");
    RTRLAB_ASSERT_MSG(bytesPerPixel > 0, "Texture upload requires a non-zero byte stride per pixel.");

    const TextureDesc& textureDesc = targetTexture->GetDesc();
    RTRLAB_ASSERT_MSG(textureDesc.m_Extent.m_Width > 0 && textureDesc.m_Extent.m_Height > 0 &&
                          textureDesc.m_Extent.m_Depth > 0,
                      "Texture upload requires a non-zero target texture extent.");

    resourceStateTracker.Transition(uploadBuffer, BufferState::CopySource);
    resourceStateTracker.Transition(targetTexture, TextureState::CopyDest);
    resourceStateTracker.FlushBarriers(&commandList);

    const BufferTextureCopyRegion copyRegion{
        0,
        textureDesc.m_Extent.m_Width * bytesPerPixel,
        textureDesc.m_Extent.m_Height,
        TextureAspect::Color,
        0,
        0,
        1,
        {},
        textureDesc.m_Extent,
    };
    commandList.CopyBufferToTexture(
        uploadBuffer, targetTexture, std::span<const BufferTextureCopyRegion>(&copyRegion, 1));

    resourceStateTracker.Transition(targetTexture, finalState);
    resourceStateTracker.FlushBarriers(&commandList);
}
} // namespace RHIUpload
