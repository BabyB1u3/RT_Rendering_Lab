#pragma once

/// @file MetalCommandList.h
/// @brief Backend-private Metal command-list wrapper.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

struct MetalCommandListData;
struct MetalDeviceData;

class MetalCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    MetalCommandList();
    ~MetalCommandList() override;

    MetalCommandList(const MetalCommandList&) = delete;
    MetalCommandList& operator=(const MetalCommandList&) = delete;
    MetalCommandList(MetalCommandList&&) = delete;
    MetalCommandList& operator=(MetalCommandList&&) = delete;

    void Initialize(MetalDeviceData* deviceData);
    void Shutdown();
    void BeginRendering(const RenderingInfo& renderingInfo) override;
    void EndRendering() override;
    void BindGraphicsPipeline(GraphicsPipeline* pipeline) override;
    void BindComputePipeline(ComputePipeline* pipeline) override;
    void BindResourceSet(uint32_t setIndex, ResourceSet* resourceSet) override;
    void PushConstants(ShaderStage stageMask, uint32_t offset, uint32_t size, const void* data) override;
    void BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets = nullptr) override;
    void
    BindVertexBuffers(uint32_t firstSlot, Buffer* const* buffers, uint32_t count, const uint64_t* offsets) override;
    void BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType) override;
    void SetViewport(float x, float y, float w, float h, float zmin, float zmax) override;
    void SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override;
    void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void
    CopyBuffer(Buffer* sourceBuffer, Buffer* destinationBuffer, std::span<const BufferCopyRegion> regions) override;
    void CopyBufferToTexture(Buffer* sourceBuffer,
                             Texture* destinationTexture,
                             std::span<const BufferTextureCopyRegion> regions) override;

    bool IsRenderingActive() const { return m_IsRendering; }

private:
    MetalCommandListData* m_Data = nullptr;
};
