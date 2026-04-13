#pragma once

/// @file RHICommandList.h
/// @brief Public RHI command-recording and render-pass description types.

#include <cstdint>
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
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct ClearDepthStencil
{
    float depth = 1.0f;
    uint8_t stencil = 0;
};

struct ColorAttachmentInfo
{
    TextureView *view = nullptr;
    LoadOp loadOp = LoadOp::Load;
    StoreOp storeOp = StoreOp::Store;
    ClearColor clearValue;
};

struct DepthAttachmentInfo
{
    TextureView *view = nullptr;
    LoadOp loadOp = LoadOp::Load;
    StoreOp storeOp = StoreOp::Store;
    ClearDepthStencil clearValue;
};

struct Rect2D
{
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct RenderingInfo
{
    std::vector<ColorAttachmentInfo> colorAttachments;
    DepthAttachmentInfo depthAttachment;
    Rect2D renderArea;
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

    virtual void beginRendering(const RenderingInfo &renderingInfo) = 0;
    virtual void endRendering() = 0;

    virtual void bindGraphicsPipeline(GraphicsPipeline *pipeline) = 0;
    virtual void bindComputePipeline(ComputePipeline *pipeline) = 0;

    virtual void bindResourceSet(uint32_t setIndex, ResourceSet *resourceSet) = 0;
    virtual void pushConstants(ShaderStage stageMask, uint32_t offset, uint32_t size, const void *data) = 0;

    virtual void bindMesh(const MeshBinding &meshBinding, const uint64_t *vertexOffsets = nullptr) = 0;
    virtual void bindVertexBuffers(uint32_t firstSlot, Buffer *const *buffers, uint32_t count, const uint64_t *offsets) = 0;
    virtual void bindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType indexType) = 0;

    virtual void setViewport(float x, float y, float w, float h, float zmin, float zmax) = 0;
    virtual void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h) = 0;

    virtual void draw(uint32_t vertexCount, uint32_t firstVertex) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset) = 0;

    virtual void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) = 0;

    virtual void textureBarrier(Texture *texture, TextureState oldState, TextureState newState, ShaderStage srcStage, ShaderStage dstStage) = 0;
    virtual void bufferBarrier(Buffer *buffer, BufferState oldState, BufferState newState, ShaderStage srcStage, ShaderStage dstStage) = 0;
};

class ResourceStateTracker
{
public:
    void transition(Texture *texture, TextureState newState);
    void transition(Buffer *buffer, BufferState newState);
    void flushBarriers(CommandList *commandList);
    void reset();
};
