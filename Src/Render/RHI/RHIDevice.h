#pragma once

/// @file RHIDevice.h
/// @brief Public RHI device and frame-lifetime interfaces.

#include "Render/RHI/RHICommandList.h"

class FrameContext
{
public:
    virtual ~FrameContext() = default;
};

class Device
{
public:
    virtual ~Device() = default;

    virtual Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc,
                                             const NativeWindowHandle& nativeWindowHandle) = 0;

    virtual Scope<Buffer> CreateBuffer(const BufferDesc& desc) = 0;
    virtual Scope<Texture> CreateTexture(const TextureDesc& desc) = 0;
    virtual Scope<TextureView> CreateTextureView(Texture* texture, const TextureViewDesc& desc) = 0;
    virtual Scope<Sampler> CreateSampler(const SamplerDesc& desc) = 0;

    virtual Scope<ShaderProgram> CreateShaderProgram(const CompiledShaderProgramDesc& desc) = 0;
    virtual Scope<PipelineLayout> CreatePipelineLayout(const PipelineLayoutDesc& desc) = 0;
    virtual Scope<ResourceSet> CreateResourceSet(PipelineLayout* layout, uint32_t setIndex) = 0;

    virtual Scope<VertexInputLayout> CreateVertexInputLayout(const VertexInputLayoutDesc& desc) = 0;

    virtual Scope<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual Scope<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;

    virtual CommandList* BeginCommandList() = 0;
    virtual void Submit(CommandList* commandList) = 0;

    virtual FrameContext* BeginFrame() = 0;
    virtual void EndFrame(FrameContext* frameContext) = 0;
};
