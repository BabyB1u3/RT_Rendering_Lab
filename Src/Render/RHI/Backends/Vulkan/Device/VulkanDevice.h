#pragma once

/// @file VulkanDevice.h
/// @brief Backend-private Vulkan device wrapper for early clear/present bring-up.

#include <array>
#include <limits>

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Vulkan/Command/VulkanCommandList.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/NativeWindowHandle.h"

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
