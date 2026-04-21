#pragma once

/// @file OpenGLDevice.h
/// @brief Backend-private OpenGL RHI classes for the early clear/present bring-up path.

#include "Render/RHI/Backends/Common/RHIShellCommon.h"

class OpenGLCommandList final : public RHIInternal::ShellCommandListBase
{
public:
    void BeginRendering(const RenderingInfo& renderingInfo) override;
    void EndRendering() override;
    void BindGraphicsPipeline(GraphicsPipeline* pipeline) override;
    void BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets = nullptr) override;
    void
    BindVertexBuffers(uint32_t firstSlot, Buffer* const* buffers, uint32_t count, const uint64_t* offsets) override;
    void BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;

    const RenderingInfo& GetRenderingInfo() const { return m_RenderingInfo; }
    bool IsRenderingActive() const { return m_IsRendering; }
};

class OpenGLSwapchain final : public RHIInternal::ShellSwapchainBase
{
public:
    OpenGLSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);

    // Compatibility-backend present stays backend-owned even though GLFW created the
    // context. This keeps Window free of a public SwapBuffers-style present API.
    void Present(uint32_t imageIndex) override;
};

class OpenGLDevice final : public RHIInternal::ShellDeviceBase
{
public:
    Scope<Swapchain> CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle) override;
    Scope<Buffer> CreateBuffer(const BufferDesc& desc) override;
    Scope<Texture> CreateTexture(const TextureDesc& desc) override;
    Scope<TextureView> CreateTextureView(Texture* texture, const TextureViewDesc& desc) override;
    Scope<Sampler> CreateSampler(const SamplerDesc& desc) override;
    Scope<ShaderProgram> CreateShaderProgram(const CompiledShaderProgramDesc& desc) override;
    Scope<VertexInputLayout> CreateVertexInputLayout(const VertexInputLayoutDesc& desc) override;
    Scope<GraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
    // TRANSITIONAL(M3): Minimal host upload path for the hello-triangle bring-up.
    // This will be replaced once renderer-owned uploads stop reaching into the
    // device directly.
    void WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size);

    CommandList* BeginCommandList() override;
    void Submit(CommandList* commandList) override;
    FrameContext* BeginFrame() override;

private:
    OpenGLCommandList m_CommandList;
    RHIInternal::ShellFrameContext m_FrameContext;
};
