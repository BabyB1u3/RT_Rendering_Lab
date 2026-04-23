#pragma once

/// @file VulkanDevice.h
/// @brief Backend-private Vulkan RHI classes for early clear/present bring-up.

#include <array>
#include <limits>
#include <vector>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Vulkan/VulkanCommon.h"

class VulkanDevice;
class VulkanSwapchainTexture;
class VulkanSwapchainImageView;

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

class VulkanSwapchain final : public Swapchain
{
public:
    VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);
    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    uint32_t AcquireNextImage() override;
    Texture* GetImage(uint32_t imageIndex) const override;
    TextureView* GetImageView(uint32_t imageIndex) const override;
    void Present(uint32_t imageIndex) override;
    void Resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t GetWidth() const override { return m_Desc.m_Width; }
    uint32_t GetHeight() const override { return m_Desc.m_Height; }
    Format GetFormat() const override { return m_Desc.m_Format; }
    uint32_t GetImageCount() const override { return static_cast<uint32_t>(m_Images.size()); }

    VkSwapchainKHR GetVkSwapchain() const { return m_Swapchain; }
    VkFormat GetVkFormat() const { return m_VkFormat; }

private:
    void RecreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void DestroySwapchain();
    TextureDesc BuildSwapchainImageDesc() const;

private:
    // Now used by the real Vulkan swapchain ownership path.
    VulkanDevice& m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkFormat m_VkFormat = VK_FORMAT_B8G8R8A8_UNORM;
    std::vector<Scope<VulkanSwapchainTexture>> m_Images;
    std::vector<Scope<VulkanSwapchainImageView>> m_ImageViews;
};

class VulkanDevice final : public RHIInternal::ShellDeviceBase
{
public:
    VulkanDevice();
    ~VulkanDevice() override;

    Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle) override;
    Scope<Buffer> CreateBuffer(const BufferDesc& desc) override;
    Scope<Texture> CreateTexture(const TextureDesc& desc) override;
    Scope<TextureView> CreateTextureView(Texture* texture, const TextureViewDesc& desc) override;
    Scope<Sampler> CreateSampler(const SamplerDesc& desc) override;
    Scope<ShaderProgram> CreateShaderProgram(const CompiledShaderProgramDesc& desc) override;
    Scope<PipelineLayout> CreatePipelineLayout(const PipelineLayoutDesc& desc) override;
    Scope<ResourceSet> CreateResourceSet(PipelineLayout* layout, uint32_t setIndex) override;
    Scope<VertexInputLayout> CreateVertexInputLayout(const VertexInputLayoutDesc& desc) override;
    Scope<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
    Scope<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override;
    void WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size) override;

    CommandList* BeginCommandList() override;
    void Submit(CommandList* commandList) override;
    FrameContext* BeginFrame() override;
    void EndFrame(FrameContext* frameContext) override;

    VkInstance GetVkInstance() const { return m_Instance; }
    VkPhysicalDevice GetVkPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetVkDevice() const { return m_Device; }
    VkSurfaceKHR GetVkSurface() const { return m_Surface; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }
    uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    uint32_t GetPresentQueueFamily() const { return m_PresentQueueFamily; }
    VkCommandPool GetVkCommandPool() const { return m_CommandPool; }
    VkSemaphore GetCurrentImageAvailableSemaphore() const;
    VkSemaphore GetCurrentRenderFinishedSemaphore() const;
    void RecycleCurrentRenderFinishedSemaphore();
    void AdvanceFrameSync();

private:
    struct FrameUploadArena
    {
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VmaAllocation_T* m_Allocation = nullptr;
        void* m_MappedData = nullptr;
        bool m_RequiresUnmap = false;
        uint64_t m_Capacity = 0;
        uint64_t m_Head = 0;
        uint64_t m_Serial = 0;
    };

    friend class VulkanCommandList;

    struct FrameSync
    {
        VkSemaphore m_ImageAvailable = VK_NULL_HANDLE;
        VkSemaphore m_RenderFinished = VK_NULL_HANDLE;
        VkFence m_InFlightFence = VK_NULL_HANDLE;
    };

    FrameSync& GetCurrentFrameSync();
    const FrameSync& GetCurrentFrameSync() const;
    void InitializeFrameUploadArenas();
    void ShutdownFrameUploadArenas();
    void ResetCurrentFrameUploadArena();
    void PrepareResourceSetForBinding(ResourceSet* resourceSet);
    void InitializeInstance();
    void InitializeDeviceObjects();
    void InitializeDeviceObjectsForSurface(VkSurfaceKHR surface);
    void InitializeAllocator();
    void InitializeFrameSyncObjects();
    void ShutdownAllocator();
    void ShutdownDeviceObjects();
    void ShutdownFrameSyncObjects();
    void InitializePresentationObjects(const NativeWindowHandle& nativeWindowHandle);
    void ShutdownPresentationObjects();

private:
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VmaAllocator_T* m_Allocator = nullptr;
    uint32_t m_GraphicsQueueFamily = std::numeric_limits<uint32_t>::max();
    uint32_t m_PresentQueueFamily = std::numeric_limits<uint32_t>::max();
    NativeWindowHandle m_NativeWindowHandle{};
    bool m_HasDeviceObjects = false;
    bool m_HasPresentationObjects = false;
    // Bring-up path currently records into a single VulkanCommandList/command buffer,
    // so only one frame may be in flight safely. Expanding this back to multiple
    // frames requires per-frame command buffers (or an equivalent ownership model).
    std::array<FrameSync, 1> m_FrameSyncObjects{};
    std::array<FrameUploadArena, 1> m_FrameUploadArenas{};
    uint32_t m_CurrentFrameSlot = 0;
    bool m_FrameInProgress = false;
    bool m_FrameSubmitted = false;
    uint64_t m_MinUniformBufferOffsetAlignment = 1;

    VulkanCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
