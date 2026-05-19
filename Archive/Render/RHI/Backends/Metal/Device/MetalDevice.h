#pragma once

/// @file MetalDevice.h
/// @brief Backend-private Metal device wrapper for early clear/present bring-up.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"
#include "Render/RHI/Backends/Metal/Command/MetalCommandList.h"

struct MetalCommandListData;
struct MetalSwapchainData;
struct MetalDeviceData;

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
