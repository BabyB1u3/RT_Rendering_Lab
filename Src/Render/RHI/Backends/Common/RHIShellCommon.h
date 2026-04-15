#pragma once

/// @file RHIShellCommon.h
/// @brief Shared implementation helpers for backend-private RHI skeleton objects.

#include <unordered_map>
#include <vector>

#include "Render/RHI/RHIDevice.h"

namespace RHIInternal
{
bool isNativeWindowHandleValid(const NativeWindowHandle& nativeWindowHandle);
SwapchainDesc sanitizeSwapchainDesc(const SwapchainDesc& desc);
PipelineLayoutDesc buildPipelineLayoutDescFromReflection(const ShaderReflectionData& reflection);

class ShellFrameContext : public FrameContext
{
};

class ShellBuffer final : public Buffer
{
public:
    explicit ShellBuffer(const BufferDesc& desc) : m_Desc(desc) {}

    const BufferDesc& getDesc() const override { return m_Desc; }

private:
    BufferDesc m_Desc;
};

class ShellTexture final : public Texture
{
public:
    explicit ShellTexture(const TextureDesc& desc) : m_Desc(desc) {}

    const TextureDesc& getDesc() const override { return m_Desc; }

private:
    TextureDesc m_Desc;
};

class ShellTextureView final : public TextureView
{
public:
    ShellTextureView(Texture* texture, const TextureViewDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    Texture* getTexture() const override { return m_Texture; }
    const TextureViewDesc& getDesc() const override { return m_Desc; }

private:
    Texture* m_Texture = nullptr;
    TextureViewDesc m_Desc;
};

class ShellSampler final : public Sampler
{
public:
    explicit ShellSampler(const SamplerDesc& desc) : m_Desc(desc) {}

    const SamplerDesc& getDesc() const override { return m_Desc; }

private:
    SamplerDesc m_Desc;
};

class ShellPipelineLayout final : public PipelineLayout
{
public:
    explicit ShellPipelineLayout(const PipelineLayoutDesc& desc) : m_Desc(desc) {}

    const PipelineLayoutDesc& getDesc() const override { return m_Desc; }

private:
    PipelineLayoutDesc m_Desc;
};

class ShellVertexInputLayout final : public VertexInputLayout
{
public:
    explicit ShellVertexInputLayout(const VertexInputLayoutDesc& desc) : m_Desc(desc) {}

    const VertexInputLayoutDesc& getDesc() const override { return m_Desc; }

private:
    VertexInputLayoutDesc m_Desc;
};

class ShellShaderProgram final : public ShaderProgram
{
public:
    // Milestone 1 keeps shader-program shells intentionally thin. Reflection is stored
    // so the public interface is satisfied, but backend code generation still belongs
    // to the later shader milestone.
    explicit ShellShaderProgram(const CompiledShaderProgramDesc& desc) : m_Reflection(desc.reflection) {}

    const ShaderReflectionData& getReflection() const override { return m_Reflection; }
    PipelineLayoutDesc derivePipelineLayoutDesc() const override;

private:
    ShaderReflectionData m_Reflection;
};

class ShellGraphicsPipeline final : public GraphicsPipeline
{
public:
    explicit ShellGraphicsPipeline(const GraphicsPipelineDesc& desc) : m_Desc(desc) {}

    const GraphicsPipelineDesc& getDesc() const override { return m_Desc; }

private:
    GraphicsPipelineDesc m_Desc;
};

class ShellComputePipeline final : public ComputePipeline
{
public:
    explicit ShellComputePipeline(const ComputePipelineDesc& desc) : m_Desc(desc) {}

    const ComputePipelineDesc& getDesc() const override { return m_Desc; }

private:
    ComputePipelineDesc m_Desc;
};

class ShellResourceSet final : public ResourceSet
{
public:
    ShellResourceSet(PipelineLayout* layout, uint32_t setIndex);

    PipelineLayout* getLayout() const override { return m_Layout; }
    uint32_t getSetIndex() const override { return m_SetIndex; }

    ParameterBlockData& constants() override { return m_Constants; }
    const ParameterBlockData& constants() const override { return m_Constants; }

    void setBuffer(uint32_t binding, const BufferBinding& bufferBinding) override;
    void setTexture(uint32_t binding, const TextureBinding& textureBinding) override;
    void setSampler(uint32_t binding, const SamplerBinding& samplerBinding) override;

    uint32_t version() const override { return m_Version; }

private:
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
    void beginRendering(const RenderingInfo& renderingInfo) override;
    void endRendering() override;

    void bindGraphicsPipeline(GraphicsPipeline* pipeline) override;
    void bindComputePipeline(ComputePipeline* pipeline) override;

    void bindResourceSet(uint32_t setIndex, ResourceSet* resourceSet) override;
    void pushConstants(ShaderStage stageMask, uint32_t offset, uint32_t size, const void* data) override;

    void bindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets = nullptr) override;
    void
    bindVertexBuffers(uint32_t firstSlot, Buffer* const* buffers, uint32_t count, const uint64_t* offsets) override;
    void bindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType) override;

    void setViewport(float x, float y, float w, float h, float zmin, float zmax) override;
    void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) override;

    void draw(uint32_t vertexCount, uint32_t firstVertex) override;
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) override;

    void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;

    void textureBarrier(Texture* texture,
                        TextureState oldState,
                        TextureState newState,
                        ShaderStage srcStage,
                        ShaderStage dstStage) override;
    void bufferBarrier(Buffer* buffer,
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

    uint32_t acquireNextImage() override;
    Texture* getImage(uint32_t imageIndex) const override;
    TextureView* getImageView(uint32_t imageIndex) const override;
    void present(uint32_t imageIndex) override;
    void resize(uint32_t newWidth, uint32_t newHeight) override;
    uint32_t width() const override { return m_Desc.width; }
    uint32_t height() const override { return m_Desc.height; }
    Format format() const override { return m_Desc.format; }
    uint32_t imageCount() const override { return static_cast<uint32_t>(m_Images.size()); }

protected:
    virtual TextureDesc buildSwapchainImageDesc() const;
    void rebuildImages();

    SwapchainDesc m_Desc;
    NativeWindowHandle m_NativeWindowHandle;
    std::vector<Scope<ShellTexture>> m_Images;
    std::vector<Scope<ShellTextureView>> m_ImageViews;
    uint32_t m_NextImageIndex = 0;
};

class ShellDeviceBase : public Device
{
public:
    Scope<Buffer> createBuffer(const BufferDesc& desc) override;
    Scope<Texture> createTexture(const TextureDesc& desc) override;
    Scope<TextureView> createTextureView(Texture* texture, const TextureViewDesc& desc) override;
    Scope<Sampler> createSampler(const SamplerDesc& desc) override;

    Scope<ShaderProgram> createShaderProgram(const CompiledShaderProgramDesc& desc) override;
    Scope<PipelineLayout> createPipelineLayout(const PipelineLayoutDesc& desc) override;
    Scope<ResourceSet> createResourceSet(PipelineLayout* layout, uint32_t setIndex) override;

    Scope<VertexInputLayout> createVertexInputLayout(const VertexInputLayoutDesc& desc) override;

    Scope<GraphicsPipeline> createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
    Scope<ComputePipeline> createComputePipeline(const ComputePipelineDesc& desc) override;

    void submit(CommandList* commandList) override;
    void endFrame(FrameContext* frameContext) override;
};
} // namespace RHIInternal
