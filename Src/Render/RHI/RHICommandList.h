#pragma once

/// @file RHICommandList.h
/// @brief Public RHI command-recording and render-pass description types.

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Render/RHI/RHIPipeline.h"

enum class LoadOp
{
    Load,
    Clear,
    DontCare,
};

enum class StoreOp
{
    Store,
    DontCare,
};

struct ClearColor
{
    float m_R = 0.0f;
    float m_G = 0.0f;
    float m_B = 0.0f;
    float m_A = 1.0f;
};

struct ClearDepthStencil
{
    float m_Depth = 1.0f;
    uint8_t m_Stencil = 0;
};

struct ColorAttachmentInfo
{
    TextureView* m_View = nullptr;
    LoadOp m_LoadOp = LoadOp::Load;
    StoreOp m_StoreOp = StoreOp::Store;
    ClearColor m_ClearValue;
};

struct DepthAttachmentInfo
{
    TextureView* m_View = nullptr;
    LoadOp m_LoadOp = LoadOp::Load;
    StoreOp m_StoreOp = StoreOp::Store;
    ClearDepthStencil m_ClearValue;
};

struct Rect2D
{
    int32_t m_X = 0;
    int32_t m_Y = 0;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
};

struct RenderingInfo
{
    std::vector<ColorAttachmentInfo> m_ColorAttachments;
    DepthAttachmentInfo m_DepthAttachment;
    Rect2D m_RenderArea;
};

enum class TextureState
{
    Undefined,
    RenderTarget,
    DepthStencil,
    ShaderRead,
    ShaderReadWrite,
    CopySource,
    CopyDest,
    Present,
};

enum class BufferState
{
    Undefined,
    VertexIndex,
    UniformRead,
    StorageRead,
    StorageReadWrite,
    CopySource,
    CopyDest,
    IndirectArgument,
};

class CommandList
{
public:
    virtual ~CommandList() = default;

    virtual void BeginRendering(const RenderingInfo& renderingInfo) = 0;
    virtual void EndRendering() = 0;

    virtual void BindGraphicsPipeline(GraphicsPipeline* pipeline) = 0;
    virtual void BindComputePipeline(ComputePipeline* pipeline) = 0;

    virtual void BindResourceSet(uint32_t setIndex, ResourceSet* resourceSet) = 0;
    virtual void PushConstants(ShaderStage stageMask, uint32_t offset, uint32_t size, const void* data) = 0;

    virtual void BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets = nullptr) = 0;
    virtual void
    BindVertexBuffers(uint32_t firstSlot, Buffer* const* buffers, uint32_t count, const uint64_t* offsets) = 0;
    virtual void BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType) = 0;

    virtual void SetViewport(float x, float y, float w, float h, float zmin, float zmax) = 0;
    virtual void SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t firstVertex) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) = 0;

    virtual void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) = 0;

    // TRANSITIONAL(M4): Barrier stage masks are still conservative in the v1
    // tracker path. Future pass-graph integration will feed precise producer /
    // consumer stage masks instead of broad fallback masks.
    virtual void TextureBarrier(
        Texture* texture, TextureState oldState, TextureState newState, ShaderStage srcStage, ShaderStage dstStage) = 0;
    // TRANSITIONAL(M4): See TextureBarrier above for the future precise stage-
    // mask plan once pass-level resource usage is available.
    virtual void BufferBarrier(
        Buffer* buffer, BufferState oldState, BufferState newState, ShaderStage srcStage, ShaderStage dstStage) = 0;
};

class ResourceStateTracker
{
public:
    void Transition(Texture* texture, TextureState newState);
    void Transition(Buffer* buffer, BufferState newState);
    void FlushBarriers(CommandList* commandList);
    void Reset();

private:
    struct PendingTextureTransition
    {
        Texture* m_Texture = nullptr;
        TextureState m_OldState = TextureState::Undefined;
        TextureState m_NewState = TextureState::Undefined;
    };

    struct PendingBufferTransition
    {
        Buffer* m_Buffer = nullptr;
        BufferState m_OldState = BufferState::Undefined;
        BufferState m_NewState = BufferState::Undefined;
    };

    std::unordered_map<Texture*, TextureState> m_TextureStates;
    std::unordered_map<Buffer*, BufferState> m_BufferStates;
    std::vector<PendingTextureTransition> m_PendingTextureTransitions;
    std::vector<PendingBufferTransition> m_PendingBufferTransitions;
};
