#pragma once

/// @file MetalDevice.h
/// @brief Backend-private Metal RHI classes for early clear/present bring-up.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

struct MetalCommandListData;
struct MetalSwapchainData;
struct MetalDeviceData;
class MetalDevice;

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

class MetalSwapchain final : public Swapchain
{
public:
    MetalSwapchain(MetalDevice& device, const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);
    ~MetalSwapchain() override;

    MetalSwapchain(const MetalSwapchain&) = delete;
    MetalSwapchain& operator=(const MetalSwapchain&) = delete;
    MetalSwapchain(MetalSwapchain&&) = delete;
    MetalSwapchain& operator=(MetalSwapchain&&) = delete;

    uint32_t AcquireNextImage() override;
    Texture* GetImage(uint32_t imageIndex) const override;
    TextureView* GetImageView(uint32_t imageIndex) const override;
    void Present(uint32_t imageIndex) override;
    void Resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t GetWidth() const override { return m_Desc.m_Width; }
    uint32_t GetHeight() const override { return m_Desc.m_Height; }
    Format GetFormat() const override { return m_Desc.m_Format; }
    uint32_t GetImageCount() const override { return 1; }

private:
    void UpdateDrawableSize();
    void SyncDrawableSizeToLayer();

private:
    MetalDevice& m_Device;
    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    MetalSwapchainData* m_Data = nullptr;
    Scope<Texture> m_Image;
    Scope<TextureView> m_ImageView;
};

class MetalDevice final : public RHIInternal::ShellDeviceBase
{
public:
    MetalDevice();
    ~MetalDevice() override;

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

    MetalDeviceData* GetData() const { return m_Data; }
    void PresentDrawable(void* drawable);

private:
    MetalDeviceData* m_Data = nullptr;
    MetalCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
