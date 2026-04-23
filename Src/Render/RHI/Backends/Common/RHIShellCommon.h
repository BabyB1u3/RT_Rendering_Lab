#pragma once

/// @file RHIShellCommon.h
/// @brief Shared implementation helpers for backend-private RHI skeleton objects.

#include <unordered_map>
#include <vector>

#include "Render/RHI/RHIDevice.h"

namespace RHIInternal
{
bool IsNativeWindowHandleValid(const NativeWindowHandle& nativeWindowHandle);
BufferDesc SanitizeBufferDesc(const BufferDesc& desc);
SwapchainDesc SanitizeSwapchainDesc(const SwapchainDesc& desc);
TextureDesc SanitizeTextureDesc(const TextureDesc& desc);
PipelineLayoutDesc BuildPipelineLayoutDescFromReflection(const ShaderReflectionData& reflection);
std::vector<const BindingInfo*> CollectBindingInfosForSet(const PipelineLayoutDesc& desc, uint32_t setIndex);
const BindingInfo*
FindBindingInfo(const PipelineLayoutDesc& desc, uint32_t setIndex, uint32_t binding, ResourceKind kind);
const BindingInfo* FindFirstBindingInfoForSet(const PipelineLayoutDesc& desc, uint32_t setIndex, ResourceKind kind);

class ShellFrameContext : public FrameContext
{
};

class ShellBuffer final : public Buffer
{
public:
    explicit ShellBuffer(const BufferDesc& desc) : m_Desc(desc) {}

    const BufferDesc& GetDesc() const override { return m_Desc; }

private:
    BufferDesc m_Desc;
};

class ShellTexture final : public Texture
{
public:
    explicit ShellTexture(const TextureDesc& desc) : m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }

private:
    TextureDesc m_Desc;
};

class ShellTextureView final : public TextureView
{
public:
    ShellTextureView(Texture* texture, const TextureViewDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }

private:
    Texture* m_Texture = nullptr;
    TextureViewDesc m_Desc;
};

class ShellSampler final : public Sampler
{
public:
    explicit ShellSampler(const SamplerDesc& desc) : m_Desc(desc) {}

    const SamplerDesc& GetDesc() const override { return m_Desc; }

private:
    SamplerDesc m_Desc;
};

class ShellPipelineLayout final : public PipelineLayout
{
public:
    explicit ShellPipelineLayout(const PipelineLayoutDesc& desc) : m_Desc(desc) {}

    const PipelineLayoutDesc& GetDesc() const override { return m_Desc; }

private:
    PipelineLayoutDesc m_Desc;
};

class ShellVertexInputLayout final : public VertexInputLayout
{
public:
    explicit ShellVertexInputLayout(const VertexInputLayoutDesc& desc) : m_Desc(desc) {}

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }

private:
    VertexInputLayoutDesc m_Desc;
};

class ShellShaderProgram final : public ShaderProgram
{
public:
    // Milestone 1 keeps shader-program shells intentionally thin. Reflection is stored
    // so the public interface is satisfied, but backend code generation still belongs
    // to the later shader milestone.
    explicit ShellShaderProgram(const CompiledShaderProgramDesc& desc) : m_Reflection(desc.m_Reflection) {}

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override;

private:
    ShaderReflectionData m_Reflection;
};

class ShellGraphicsPipeline final : public GraphicsPipeline
{
public:
    explicit ShellGraphicsPipeline(const GraphicsPipelineDesc& desc) : m_Desc(desc) {}

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }

private:
    GraphicsPipelineDesc m_Desc;
};

class ShellComputePipeline final : public ComputePipeline
{
public:
    explicit ShellComputePipeline(const ComputePipelineDesc& desc) : m_Desc(desc) {}

    const ComputePipelineDesc& GetDesc() const override { return m_Desc; }

private:
    ComputePipelineDesc m_Desc;
};

class ShellResourceSet final : public ResourceSet
{
public:
    ShellResourceSet(PipelineLayout* layout, uint32_t setIndex);

    PipelineLayout* GetLayout() const override { return m_Layout; }
    uint32_t GetSetIndex() const override { return m_SetIndex; }

    const ParameterBlockData& GetConstants() const override { return m_Constants; }
    void SetConstantDataRaw(uint32_t offset, const void* data, size_t size) override;

    void SetBuffer(uint32_t binding, const BufferBinding& bufferBinding) override;
    void SetTexture(uint32_t binding, const TextureBinding& textureBinding) override;
    void SetSampler(uint32_t binding, const SamplerBinding& samplerBinding) override;

    uint32_t GetVersion() const override { return m_Version; }

private:
    const BindingInfo& RequireBindingInfo(uint32_t binding, ResourceKind kind) const;
    void ValidateConstantBindingExists() const;

    PipelineLayout* m_Layout = nullptr;
    uint32_t m_SetIndex = 0;
    ParameterBlockData m_Constants;
    std::unordered_map<uint32_t, BufferBinding> m_BufferBindings;
    std::unordered_map<uint32_t, TextureBinding> m_TextureBindings;
    std::unordered_map<uint32_t, SamplerBinding> m_SamplerBindings;
    uint32_t m_Version = 0;
};

class ShellCommandListBase : public CommandList
{
public:
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

    void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;

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

protected:
    RenderingInfo m_RenderingInfo;
    bool m_IsRendering = false;
    GraphicsPipeline* m_GraphicsPipeline = nullptr;
    ComputePipeline* m_ComputePipeline = nullptr;
    std::unordered_map<uint32_t, ResourceSet*> m_ResourceSets;
    MeshBinding m_MeshBinding;
    std::vector<uint64_t> m_VertexOffsets;
    Buffer* m_IndexBuffer = nullptr;
    uint64_t m_IndexOffset = 0;
    IndexType m_IndexType = IndexType::UInt32;
    Rect2D m_Scissor;
    float m_Viewport[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    std::vector<uint8_t> m_PushConstants;
    uint32_t m_LastDrawVertexCount = 0;
    uint32_t m_LastDrawFirstVertex = 0;
    uint32_t m_LastDrawIndexedCount = 0;
    uint32_t m_LastDrawFirstIndex = 0;
    int32_t m_LastDrawVertexOffset = 0;
    uint32_t m_LastDispatchX = 0;
    uint32_t m_LastDispatchY = 0;
    uint32_t m_LastDispatchZ = 0;
};

class ShellSwapchainBase : public Swapchain
{
public:
    ShellSwapchainBase(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle);

    uint32_t AcquireNextImage() override;
    Texture* GetImage(uint32_t imageIndex) const override;
    TextureView* GetImageView(uint32_t imageIndex) const override;
    void Present(uint32_t imageIndex) override;
    void Resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t GetWidth() const override { return m_Desc.m_Width; }
    uint32_t GetHeight() const override { return m_Desc.m_Height; }
    Format GetFormat() const override { return m_Desc.m_Format; }
    uint32_t GetImageCount() const override { return static_cast<uint32_t>(m_Images.size()); }

protected:
    virtual TextureDesc BuildSwapchainImageDesc() const;
    void RebuildImages();

    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    std::vector<Scope<ShellTexture>> m_Images;
    std::vector<Scope<ShellTextureView>> m_ImageViews;
    uint32_t m_NextImageIndex = 0;
};

class ShellDeviceBase : public Device
{
public:
    // ShellDeviceBase remains the default shell/null/testing implementation for
    // backends that do not override a given RHI object path yet. Real graphics
    // backends may replace some or all of these factories with native objects.
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

    void Submit(CommandList* commandList) override;
    void EndFrame(FrameContext* frameContext) override;
};
} // namespace RHIInternal
