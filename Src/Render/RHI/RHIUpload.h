#pragma once

/// @file RHIUpload.h
/// @brief Small explicit-upload helpers shared by early renderer and demo paths.

#include <cstdint>

#include "Core/Util/Base.h"
#include "Render/RHI/RHI.h"

namespace RHIUpload
{
Scope<Buffer> CreateUploadBuffer(Device& device, const void* data, uint64_t size, const char* debugName);

void CreateStaticBufferPair(Device& device,
                            const void* data,
                            uint64_t size,
                            BufferUsage targetUsage,
                            const char* targetDebugName,
                            const char* uploadDebugName,
                            Scope<Buffer>& targetBuffer,
                            Scope<Buffer>& uploadBuffer);

void UploadStaticBufferPair(CommandList& commandList,
                            ResourceStateTracker& resourceStateTracker,
                            Buffer* uploadBuffer,
                            Buffer* targetBuffer,
                            uint64_t size,
                            BufferState finalState);

void UploadFullTexture(CommandList& commandList,
                       ResourceStateTracker& resourceStateTracker,
                       Buffer* uploadBuffer,
                       Texture* targetTexture,
                       uint32_t bytesPerPixel,
                       TextureState finalState);
} // namespace RHIUpload
