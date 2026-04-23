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
    // Lifetime contract: a Device must outlive the RHI objects it creates.
    // Individual backend destructors may null-guard native handles as a defensive
    // fallback, but callers should still destroy Buffer/Texture/Sampler/Pipeline/
    // Swapchain objects before destroying the Device that owns them.

    virtual Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc,
                                             const NativeWindowHandle& nativeWindowHandle) = 0;

    virtual Scope<Buffer> CreateBuffer(const BufferDesc& desc) = 0;
    virtual Scope<Texture> CreateTexture(const TextureDesc& desc) = 0;
    // Swapchain images expose their canonical views via Swapchain::GetImageView().
    // Device::CreateTextureView is reserved for device-created textures.
    virtual Scope<TextureView> CreateTextureView(Texture* texture, const TextureViewDesc& desc) = 0;
    virtual Scope<Sampler> CreateSampler(const SamplerDesc& desc) = 0;

    virtual Scope<ShaderProgram> CreateShaderProgram(const CompiledShaderProgramDesc& desc) = 0;
    virtual Scope<PipelineLayout> CreatePipelineLayout(const PipelineLayoutDesc& desc) = 0;
    virtual Scope<ResourceSet> CreateResourceSet(PipelineLayout* layout, uint32_t setIndex) = 0;

    virtual Scope<VertexInputLayout> CreateVertexInputLayout(const VertexInputLayoutDesc& desc) = 0;

    virtual Scope<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual Scope<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
    // TRANSITIONAL(M3): Compatibility host upload hook used by early demos.
    // New renderer-facing upload paths should prefer explicit copy recording
    // on CommandList plus staging resources instead of reaching into Device
    // for direct writes. This remains only until renderer-owned upload
    // scheduling replaces the demo path completely.
    virtual void WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size) = 0;

    virtual CommandList* BeginCommandList() = 0;
    // Submit seals the current command list for the frame and makes its work
    // eligible for backend submission/presentation flow. Backends may still
    // defer the final present-coupled queue operation until Swapchain::Present(),
    // but callers must treat the command list as no longer recordable after Submit.
    virtual void Submit(CommandList* commandList) = 0;

    virtual FrameContext* BeginFrame() = 0;
    virtual void EndFrame(FrameContext* frameContext) = 0;
};
