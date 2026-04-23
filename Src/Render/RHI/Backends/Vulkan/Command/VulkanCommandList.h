#pragma once

/// @file VulkanCommandList.h
/// @brief Backend-private Vulkan command-list that records into a single
/// VkCommandBuffer owned by the device.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"

class VulkanDevice;

class VulkanCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    VulkanCommandList() = default;
    ~VulkanCommandList();

    VulkanCommandList(const VulkanCommandList&) = delete;
    VulkanCommandList& operator=(const VulkanCommandList&) = delete;
    VulkanCommandList(VulkanCommandList&&) = delete;
    VulkanCommandList& operator=(VulkanCommandList&&) = delete;

    void Initialize(VulkanDevice* ownerDevice, VkDevice device, VkCommandPool commandPool);
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
    void TextureBarrier(Texture* texture,
                        TextureState oldState,
                        TextureState newState,
                        ShaderStage srcStage,
                        ShaderStage dstStage) override;
    void BufferBarrier(Buffer* buffer,
                       BufferState oldState,
                       BufferState newState,
                       ShaderStage srcStage,
                       ShaderStage dstStage) override;
    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;
    void
    CopyBuffer(Buffer* sourceBuffer, Buffer* destinationBuffer, std::span<const BufferCopyRegion> regions) override;
    void CopyBufferToTexture(Buffer* sourceBuffer,
                             Texture* destinationTexture,
                             std::span<const BufferTextureCopyRegion> regions) override;

    VkCommandBuffer GetVkCommandBuffer() const { return m_CommandBuffer; }
    bool IsRenderingActive() const { return m_IsRendering; }

private:
    VulkanDevice* m_OwnerDevice = nullptr;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};
