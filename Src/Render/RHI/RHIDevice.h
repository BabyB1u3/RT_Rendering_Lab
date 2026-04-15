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

    virtual Scope<Swapchain> createSwapchain(const SwapchainDesc& desc,
                                             const NativeWindowHandle& nativeWindowHandle) = 0;

    virtual Scope<Buffer> createBuffer(const BufferDesc& desc) = 0;
    virtual Scope<Texture> createTexture(const TextureDesc& desc) = 0;
    virtual Scope<TextureView> createTextureView(Texture* texture, const TextureViewDesc& desc) = 0;
    virtual Scope<Sampler> createSampler(const SamplerDesc& desc) = 0;

    virtual Scope<ShaderProgram> createShaderProgram(const CompiledShaderProgramDesc& desc) = 0;
    virtual Scope<PipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) = 0;
    virtual Scope<ResourceSet> createResourceSet(PipelineLayout* layout, uint32_t setIndex) = 0;

    virtual Scope<VertexInputLayout> createVertexInputLayout(const VertexInputLayoutDesc& desc) = 0;

    virtual Scope<GraphicsPipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual Scope<ComputePipeline> createComputePipeline(const ComputePipelineDesc& desc) = 0;

    virtual CommandList* beginCommandList() = 0;
    virtual void submit(CommandList* commandList) = 0;

    virtual FrameContext* beginFrame() = 0;
    virtual void endFrame(FrameContext* frameContext) = 0;
};
