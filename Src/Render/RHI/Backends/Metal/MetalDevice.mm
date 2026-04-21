#include "Render/RHI/Backends/Metal/MetalDevice.h"

#include <algorithm>
#include <cstring>
#include <string>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
MTLStorageMode ToMetalStorageMode(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return MTLStorageModePrivate;
        case MemoryUsage::CpuToGpu:
        case MemoryUsage::GpuToCpu:
            return MTLStorageModeShared;
    }

    return MTLStorageModePrivate;
}

MTLResourceOptions ToMetalBufferResourceOptions(MemoryUsage memoryUsage)
{
    return MTLResourceCPUCacheModeDefaultCache |
           static_cast<MTLResourceOptions>(ToMetalStorageMode(memoryUsage) << MTLResourceStorageModeShift);
}

MTLPixelFormat ToMetalPixelFormat(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM:
            return MTLPixelFormatR8Unorm;
        case Format::RG8_UNORM:
            return MTLPixelFormatRG8Unorm;
        case Format::BGRA8_UNORM:
            return MTLPixelFormatBGRA8Unorm;
        case Format::BGRA8_SRGB:
            return MTLPixelFormatBGRA8Unorm_sRGB;
        case Format::RGBA8_UNORM:
            return MTLPixelFormatRGBA8Unorm;
        case Format::RGBA8_SRGB:
            return MTLPixelFormatRGBA8Unorm_sRGB;
        case Format::R16F:
            return MTLPixelFormatR16Float;
        case Format::RG16F:
            return MTLPixelFormatRG16Float;
        case Format::RGBA16F:
            return MTLPixelFormatRGBA16Float;
        case Format::R32F:
            return MTLPixelFormatR32Float;
        case Format::RG32F:
            return MTLPixelFormatRG32Float;
        case Format::RGBA32F:
            return MTLPixelFormatRGBA32Float;
        case Format::R32_UINT:
            return MTLPixelFormatR32Uint;
        case Format::D16_UNORM:
            return MTLPixelFormatDepth16Unorm;
        case Format::D32_SFLOAT:
            return MTLPixelFormatDepth32Float;
        case Format::D24_UNORM_S8_UINT:
            return MTLPixelFormatDepth24Unorm_Stencil8;
        case Format::D32_SFLOAT_S8_UINT:
            return MTLPixelFormatDepth32Float_Stencil8;
        default:
            RTRLAB_ASSERTF(false, "Unsupported Metal RHI format {}", static_cast<uint32_t>(format));
            return MTLPixelFormatInvalid;
    }
}

MTLTextureType ToMetalTextureType(TextureType type)
{
    switch (type)
    {
        case TextureType::Tex2D:
            return MTLTextureType2D;
        case TextureType::Tex2DArray:
            return MTLTextureType2DArray;
        case TextureType::Tex3D:
            return MTLTextureType3D;
        case TextureType::Cube:
            return MTLTextureTypeCube;
    }

    return MTLTextureType2D;
}

MTLSamplerMinMagFilter ToMetalFilter(FilterMode mode)
{
    return mode == FilterMode::Nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}

MTLSamplerMipFilter ToMetalMipFilter(MipFilterMode mode)
{
    switch (mode)
    {
        case MipFilterMode::None:
            return MTLSamplerMipFilterNotMipmapped;
        case MipFilterMode::Nearest:
            return MTLSamplerMipFilterNearest;
        case MipFilterMode::Linear:
            return MTLSamplerMipFilterLinear;
    }

    return MTLSamplerMipFilterLinear;
}

MTLSamplerAddressMode ToMetalAddressMode(AddressMode mode)
{
    switch (mode)
    {
        case AddressMode::Repeat:
            return MTLSamplerAddressModeRepeat;
        case AddressMode::MirroredRepeat:
            return MTLSamplerAddressModeMirrorRepeat;
        case AddressMode::ClampToEdge:
            return MTLSamplerAddressModeClampToEdge;
        case AddressMode::ClampToBorder:
            return MTLSamplerAddressModeClampToBorderColor;
    }

    return MTLSamplerAddressModeRepeat;
}

MTLLoadAction ToMetalLoadAction(LoadOp loadOp)
{
    switch (loadOp)
    {
        case LoadOp::Clear:
            return MTLLoadActionClear;
        case LoadOp::DontCare:
            return MTLLoadActionDontCare;
        case LoadOp::Load:
        default:
            return MTLLoadActionLoad;
    }
}

MTLStoreAction ToMetalStoreAction(StoreOp storeOp)
{
    switch (storeOp)
    {
        case StoreOp::DontCare:
            return MTLStoreActionDontCare;
        case StoreOp::Store:
        default:
            return MTLStoreActionStore;
    }
}

bool HasDebugName(const char* debugName)
{
    return debugName != nullptr && debugName[0] != '\0';
}

NSString* MakeNSString(const char* debugName)
{
    if (!HasDebugName(debugName))
        return nil;

    return [NSString stringWithUTF8String:debugName];
}

void SetMetalDebugLabel(id<MTLResource> resource, const char* debugName)
{
    NSString* label = MakeNSString(debugName);
    if (resource != nil && label != nil)
        [resource setLabel:label];
}

std::string MakeTextureViewDebugName(const Texture& texture)
{
    const char* debugName = texture.GetDesc().m_DebugName;
    if (!HasDebugName(debugName))
        return {};

    return std::string(debugName) + ".View";
}

MTLVertexFormat ToMetalVertexFormat(Format format)
{
    switch (format)
    {
        case Format::RG32F:
            return MTLVertexFormatFloat2;
        case Format::RGBA32F:
            return MTLVertexFormatFloat4;
        default:
            RTRLAB_ASSERTF(false, "Unsupported Metal vertex attribute format {}", static_cast<uint32_t>(format));
            return MTLVertexFormatInvalid;
    }
}

MTLPrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology)
{
    switch (topology)
    {
        case PrimitiveTopology::TriangleList:
            return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::TriangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveTopology::LineList:
            return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LineStrip:
            return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::PointList:
            return MTLPrimitiveTypePoint;
    }

    return MTLPrimitiveTypeTriangle;
}

MTLIndexType ToMetalIndexType(IndexType indexType)
{
    switch (indexType)
    {
        case IndexType::UInt16:
            return MTLIndexTypeUInt16;
        case IndexType::UInt32:
            return MTLIndexTypeUInt32;
    }

    return MTLIndexTypeUInt32;
}

MTLWinding ToMetalWinding(FrontFace frontFace)
{
    return frontFace == FrontFace::CW ? MTLWindingClockwise : MTLWindingCounterClockwise;
}

MTLCullMode ToMetalCullMode(CullMode cullMode)
{
    switch (cullMode)
    {
        case CullMode::None:
            return MTLCullModeNone;
        case CullMode::Front:
            return MTLCullModeFront;
        case CullMode::Back:
            return MTLCullModeBack;
    }

    return MTLCullModeBack;
}

MTLTriangleFillMode ToMetalTriangleFillMode(FillMode fillMode)
{
    return fillMode == FillMode::Wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;
}

NSString* MakeNSString(const std::vector<uint8_t>& utf8Bytes)
{
    if (utf8Bytes.empty())
        return nil;

    return [[[NSString alloc] initWithBytes:utf8Bytes.data() length:utf8Bytes.size()
                                   encoding:NSUTF8StringEncoding] autorelease];
}
} // namespace

class MetalSwapchainTexture final : public Texture
{
public:
    MetalSwapchainTexture(id<MTLTexture> texture, const TextureDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    const TextureDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTexture() const { return m_Texture; }

private:
    id<MTLTexture> m_Texture = nil;
    TextureDesc m_Desc;
};

class MetalSwapchainImageView final : public TextureView
{
public:
    MetalSwapchainImageView(Texture* texture, const TextureViewDesc& desc) : m_Texture(texture), m_Desc(desc) {}

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }

private:
    Texture* m_Texture = nullptr;
    TextureViewDesc m_Desc;
};

class MetalBuffer final : public Buffer
{
public:
    MetalBuffer(id<MTLBuffer> buffer, const BufferDesc& desc) : m_Buffer([buffer retain]), m_Desc(desc) {}

    ~MetalBuffer() override
    {
        if (m_Buffer != nil)
        {
            [m_Buffer release];
            m_Buffer = nil;
        }
    }

    const BufferDesc& GetDesc() const override { return m_Desc; }
    id<MTLBuffer> GetMetalBuffer() const { return m_Buffer; }

private:
    id<MTLBuffer> m_Buffer = nil;
    BufferDesc m_Desc;
};

class MetalTexture final : public Texture
{
public:
    MetalTexture(id<MTLTexture> texture, const TextureDesc& desc) : m_Texture([texture retain]), m_Desc(desc) {}

    ~MetalTexture() override
    {
        if (m_Texture != nil)
        {
            [m_Texture release];
            m_Texture = nil;
        }
    }

    const TextureDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTexture() const { return m_Texture; }

private:
    id<MTLTexture> m_Texture = nil;
    TextureDesc m_Desc;
};

class MetalTextureView final : public TextureView
{
public:
    MetalTextureView(Texture* texture, id<MTLTexture> textureView, const TextureViewDesc& desc)
        : m_Texture(texture), m_TextureView([textureView retain]), m_Desc(desc)
    {
    }

    ~MetalTextureView() override
    {
        if (m_TextureView != nil)
        {
            [m_TextureView release];
            m_TextureView = nil;
        }
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    id<MTLTexture> GetMetalTextureView() const { return m_TextureView; }

private:
    Texture* m_Texture = nullptr;
    id<MTLTexture> m_TextureView = nil;
    TextureViewDesc m_Desc;
};

class MetalSampler final : public Sampler
{
public:
    MetalSampler(id<MTLSamplerState> sampler, const SamplerDesc& desc) : m_Sampler([sampler retain]), m_Desc(desc) {}

    ~MetalSampler() override
    {
        if (m_Sampler != nil)
        {
            [m_Sampler release];
            m_Sampler = nil;
        }
    }

    const SamplerDesc& GetDesc() const override { return m_Desc; }
    id<MTLSamplerState> GetMetalSampler() const { return m_Sampler; }

private:
    id<MTLSamplerState> m_Sampler = nil;
    SamplerDesc m_Desc;
};

class MetalShaderProgram final : public ShaderProgram
{
public:
    struct StageFunction
    {
        ShaderStage m_Stage = ShaderStage::None;
        id<MTLFunction> m_Function = nil;
    };

    MetalShaderProgram(const CompiledShaderProgramDesc& desc, std::vector<StageFunction>&& functions)
        : m_Reflection(desc.m_Reflection), m_Functions(std::move(functions))
    {
    }

    ~MetalShaderProgram() override
    {
        for (const StageFunction& function : m_Functions)
        {
            if (function.m_Function != nil)
                [function.m_Function release];
        }
    }

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override
    {
        return RHIInternal::BuildPipelineLayoutDescFromReflection(m_Reflection);
    }

    id<MTLFunction> FindStage(ShaderStage stage) const
    {
        const auto it = std::find_if(m_Functions.begin(),
                                     m_Functions.end(),
                                     [stage](const StageFunction& function) { return function.m_Stage == stage; });
        return it != m_Functions.end() ? it->m_Function : nil;
    }

private:
    ShaderReflectionData m_Reflection;
    std::vector<StageFunction> m_Functions;
};

class MetalVertexInputLayout final : public VertexInputLayout
{
public:
    explicit MetalVertexInputLayout(const VertexInputLayoutDesc& desc) : m_Desc(desc) {}

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }

private:
    VertexInputLayoutDesc m_Desc;
};

class MetalGraphicsPipeline final : public GraphicsPipeline
{
public:
    MetalGraphicsPipeline(id<MTLRenderPipelineState> pipelineState, const GraphicsPipelineDesc& desc)
        : m_PipelineState([pipelineState retain]), m_Desc(desc)
    {
    }

    ~MetalGraphicsPipeline() override
    {
        if (m_PipelineState != nil)
        {
            [m_PipelineState release];
            m_PipelineState = nil;
        }
    }

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
    id<MTLRenderPipelineState> GetPipelineState() const { return m_PipelineState; }

private:
    id<MTLRenderPipelineState> m_PipelineState = nil;
    GraphicsPipelineDesc m_Desc;
};

const MetalShaderProgram& GetMetalShaderProgram(ShaderProgram* shaderProgram)
{
    auto* metalShaderProgram = dynamic_cast<MetalShaderProgram*>(shaderProgram);
    RTRLAB_ASSERT_MSG(metalShaderProgram != nullptr, "GraphicsPipeline requires a Metal shader program.");
    return *metalShaderProgram;
}

const MetalVertexInputLayout& GetMetalVertexInputLayout(VertexInputLayout* vertexInputLayout)
{
    auto* metalVertexInputLayout = dynamic_cast<MetalVertexInputLayout*>(vertexInputLayout);
    RTRLAB_ASSERT_MSG(metalVertexInputLayout != nullptr, "GraphicsPipeline requires a Metal vertex input layout.");
    return *metalVertexInputLayout;
}

const MetalGraphicsPipeline& GetMetalGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
    auto* metalGraphicsPipeline = dynamic_cast<MetalGraphicsPipeline*>(graphicsPipeline);
    RTRLAB_ASSERT_MSG(metalGraphicsPipeline != nullptr, "Graphics pipeline is not owned by the Metal backend.");
    return *metalGraphicsPipeline;
}

MetalBuffer& GetMetalBuffer(Buffer* buffer)
{
    auto* metalBuffer = dynamic_cast<MetalBuffer*>(buffer);
    RTRLAB_ASSERT_MSG(metalBuffer != nullptr, "Buffer is not owned by the Metal backend.");
    return *metalBuffer;
}

struct MetalDeviceData
{
    id<MTLDevice> m_Device = nil;
    id<MTLCommandQueue> m_CommandQueue = nil;
    id<MTLCommandBuffer> m_CurrentCommandBuffer = nil;
    NSAutoreleasePool* m_FrameAutoreleasePool = nil;
    bool m_FrameInProgress = false;
    bool m_FrameSubmitted = false;
};

struct MetalCommandListData
{
    MetalDeviceData* m_DeviceData = nullptr;
    id<MTLRenderCommandEncoder> m_RenderEncoder = nil;
};

struct MetalSwapchainData
{
    CAMetalLayer* m_Layer = nil;
    id<CAMetalDrawable> m_CurrentDrawable = nil;
};

MetalCommandList::MetalCommandList() : m_Data(new MetalCommandListData()) {}

MetalCommandList::~MetalCommandList()
{
    Shutdown();
    delete m_Data;
    m_Data = nullptr;
}

void MetalCommandList::Initialize(MetalDeviceData* deviceData)
{
    Shutdown();
    m_Data->m_DeviceData = deviceData;
}

void MetalCommandList::Shutdown()
{
    if (m_Data == nullptr)
        return;

    if (m_Data->m_RenderEncoder != nil)
    {
        [m_Data->m_RenderEncoder endEncoding];
        [m_Data->m_RenderEncoder release];
        m_Data->m_RenderEncoder = nil;
    }

    m_Data->m_DeviceData = nullptr;
}

void MetalCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_DeviceData != nullptr,
                      "Metal command list must be initialized before rendering.");
    RTRLAB_ASSERT_MSG(m_Data->m_DeviceData->m_CurrentCommandBuffer != nil,
                      "Metal rendering requires an active command buffer.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_ColorAttachments.size() == 1,
                      "Early Metal bring-up currently supports exactly one color attachment.");
    RTRLAB_ASSERT_MSG(renderingInfo.m_DepthAttachment.m_View == nullptr,
                      "Early Metal bring-up does not support depth attachments yet.");

    const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
    auto* imageView = dynamic_cast<MetalSwapchainImageView*>(colorAttachment.m_View);
    RTRLAB_ASSERT_MSG(imageView != nullptr, "Metal BeginRendering currently expects a swapchain image view.");

    auto* texture = dynamic_cast<MetalSwapchainTexture*>(imageView->GetTexture());
    RTRLAB_ASSERT_MSG(texture != nullptr, "Metal BeginRendering currently expects a swapchain texture.");

    MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = texture->GetMetalTexture();
    renderPassDescriptor.colorAttachments[0].loadAction = ToMetalLoadAction(colorAttachment.m_LoadOp);
    renderPassDescriptor.colorAttachments[0].storeAction = ToMetalStoreAction(colorAttachment.m_StoreOp);
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(colorAttachment.m_ClearValue.m_R,
                                                                            colorAttachment.m_ClearValue.m_G,
                                                                            colorAttachment.m_ClearValue.m_B,
                                                                            colorAttachment.m_ClearValue.m_A);

    m_Data->m_RenderEncoder =
        [[m_Data->m_DeviceData->m_CurrentCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor] retain];
    RTRLAB_ASSERT_MSG(m_Data->m_RenderEncoder != nil, "Failed to create the Metal render command encoder.");
}

void MetalCommandList::EndRendering()
{
    RTRLAB_ASSERT_MSG(m_IsRendering, "Metal EndRendering requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal EndRendering requires an active render encoder.");

    [m_Data->m_RenderEncoder endEncoding];
    [m_Data->m_RenderEncoder release];
    m_Data->m_RenderEncoder = nil;

    ShellCommandListBase::EndRendering();
}

void MetalCommandList::BindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    ShellCommandListBase::BindGraphicsPipeline(pipeline);

    if (pipeline == nullptr)
        return;

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal graphics pipelines require an active render encoder.");
    const MetalGraphicsPipeline& metalPipeline = GetMetalGraphicsPipeline(pipeline);
    [m_Data->m_RenderEncoder setRenderPipelineState:metalPipeline.GetPipelineState()];
    [m_Data->m_RenderEncoder setCullMode:ToMetalCullMode(metalPipeline.GetDesc().m_RasterState.m_CullMode)];
    [m_Data->m_RenderEncoder setFrontFacingWinding:ToMetalWinding(metalPipeline.GetDesc().m_RasterState.m_FrontFace)];
    [m_Data->m_RenderEncoder
        setTriangleFillMode:ToMetalTriangleFillMode(metalPipeline.GetDesc().m_RasterState.m_FillMode)];
}

void MetalCommandList::BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets)
{
    ShellCommandListBase::BindMesh(meshBinding, vertexOffsets);

    if (!meshBinding.m_VertexBuffers.empty())
    {
        BindVertexBuffers(0,
                          meshBinding.m_VertexBuffers.data(),
                          static_cast<uint32_t>(meshBinding.m_VertexBuffers.size()),
                          vertexOffsets);
    }

    if (meshBinding.m_IndexBuffer != nullptr)
        BindIndexBuffer(meshBinding.m_IndexBuffer, 0, meshBinding.m_IndexType);
}

void MetalCommandList::BindVertexBuffers(uint32_t firstSlot,
                                         Buffer* const* buffers,
                                         uint32_t count,
                                         const uint64_t* offsets)
{
    ShellCommandListBase::BindVertexBuffers(firstSlot, buffers, count, offsets);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal vertex buffers require an active render encoder.");

    for (uint32_t index = 0; index < count; ++index)
    {
        RTRLAB_ASSERT_MSG(buffers[index] != nullptr, "Metal BindVertexBuffers requires non-null buffers.");
        const uint64_t offset = offsets != nullptr ? offsets[index] : 0;
        [m_Data->m_RenderEncoder setVertexBuffer:GetMetalBuffer(buffers[index]).GetMetalBuffer()
                                          offset:static_cast<NSUInteger>(offset)
                                         atIndex:firstSlot + index];
    }
}

void MetalCommandList::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
{
    ShellCommandListBase::BindIndexBuffer(buffer, offset, indexType);
}

void MetalCommandList::SetViewport(float x, float y, float w, float h, float zmin, float zmax)
{
    ShellCommandListBase::SetViewport(x, y, w, h, zmin, zmax);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal SetViewport requires an active render encoder.");

    MTLViewport viewport;
    viewport.originX = x;
    viewport.originY = y;
    viewport.width = w;
    viewport.height = h;
    viewport.znear = zmin;
    viewport.zfar = zmax;
    [m_Data->m_RenderEncoder setViewport:viewport];
}

void MetalCommandList::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    ShellCommandListBase::SetScissor(x, y, w, h);

    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal SetScissor requires an active render encoder.");

    MTLScissorRect scissor;
    scissor.x = static_cast<NSUInteger>(std::max<int32_t>(x, 0));
    scissor.y = static_cast<NSUInteger>(std::max<int32_t>(y, 0));
    scissor.width = static_cast<NSUInteger>(w);
    scissor.height = static_cast<NSUInteger>(h);
    [m_Data->m_RenderEncoder setScissorRect:scissor];
}

void MetalCommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
{
    ShellCommandListBase::DrawIndexed(indexCount, firstIndex, vertexOffset);

    RTRLAB_ASSERT_MSG(m_IsRendering, "Metal DrawIndexed requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_RenderEncoder != nil,
                      "Metal DrawIndexed requires an active render encoder.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "Metal DrawIndexed requires a bound graphics pipeline.");
    RTRLAB_ASSERT_MSG(m_IndexBuffer != nullptr, "Metal DrawIndexed requires a bound index buffer.");

    const MetalGraphicsPipeline& metalPipeline = GetMetalGraphicsPipeline(m_GraphicsPipeline);
    const NSUInteger indexSize = m_IndexType == IndexType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    const NSUInteger indexBufferOffset =
        static_cast<NSUInteger>(m_IndexOffset + static_cast<uint64_t>(firstIndex) * indexSize);

    [m_Data->m_RenderEncoder drawIndexedPrimitives:ToMetalPrimitiveType(metalPipeline.GetDesc().m_Topology)
                                        indexCount:indexCount
                                         indexType:ToMetalIndexType(m_IndexType)
                                       indexBuffer:GetMetalBuffer(m_IndexBuffer).GetMetalBuffer()
                                 indexBufferOffset:indexBufferOffset
                                     instanceCount:1
                                        baseVertex:vertexOffset
                                      baseInstance:0];
}

MetalSwapchain::MetalSwapchain(MetalDevice& device,
                               const SwapchainDesc& desc,
                               const NativeWindowHandle& nativeWindowHandle)
    : m_Device(device),
      m_Desc(RHIInternal::SanitizeSwapchainDesc(desc)),
      m_NativeWindowHandle(nativeWindowHandle),
      m_Data(new MetalSwapchainData())
{
    RTRLAB_ASSERT_MSG(RHIInternal::IsNativeWindowHandleValid(nativeWindowHandle),
                      "Native window handle is incomplete.");
    RTRLAB_ASSERT_MSG(nativeWindowHandle.m_Layer != nullptr, "Metal swapchain creation requires a valid CAMetalLayer.");

    m_Data->m_Layer = [(__bridge CAMetalLayer*)nativeWindowHandle.m_Layer retain];
    // Window/platform code only supplies the presentation surface. Metal-specific layer
    // configuration is owned here by the backend swapchain.
    m_Data->m_Layer.device = m_Device.GetData()->m_Device;
    m_Data->m_Layer.pixelFormat = ToMetalPixelFormat(m_Desc.m_Format);
    m_Data->m_Layer.framebufferOnly = YES;
    UpdateDrawableSize();
}

MetalSwapchain::~MetalSwapchain()
{
    m_ImageView.reset();
    m_Image.reset();

    if (m_Data != nullptr)
    {
        if (m_Data->m_CurrentDrawable != nil)
        {
            [m_Data->m_CurrentDrawable release];
            m_Data->m_CurrentDrawable = nil;
        }

        if (m_Data->m_Layer != nil)
        {
            [m_Data->m_Layer release];
            m_Data->m_Layer = nil;
        }

        delete m_Data;
        m_Data = nullptr;
    }
}

uint32_t MetalSwapchain::AcquireNextImage()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Layer != nil,
                      "Metal swapchain layer must be valid before acquiring a drawable.");

    id<CAMetalDrawable> drawable = [m_Data->m_Layer nextDrawable];
    RTRLAB_ASSERT_MSG(drawable != nil,
                      "Failed to acquire a CAMetalDrawable from the Metal layer. Window minimized or drawable "
                      "acquisition timed out?");

    if (m_Data->m_CurrentDrawable != nil)
        [m_Data->m_CurrentDrawable release];
    m_Data->m_CurrentDrawable = [drawable retain];

    TextureDesc imageDesc;
    imageDesc.m_Type = TextureType::Tex2D;
    imageDesc.m_Format = m_Desc.m_Format;
    imageDesc.m_Extent = Extent3D{m_Desc.m_Width, m_Desc.m_Height, 1};
    imageDesc.m_MipLevels = 1;
    imageDesc.m_ArrayLayers = 1;
    imageDesc.m_UsageMask = TextureUsage::RenderTarget;
    imageDesc.m_DebugName = "MetalSwapchainImage";
    SetMetalDebugLabel(drawable.texture, imageDesc.m_DebugName);

    auto image = CreateScope<MetalSwapchainTexture>(drawable.texture, imageDesc);

    TextureViewDesc viewDesc;
    viewDesc.m_Type = TextureType::Tex2D;
    viewDesc.m_Format = m_Desc.m_Format;
    viewDesc.m_Aspect = TextureAspect::Color;

    m_ImageView = CreateScope<MetalSwapchainImageView>(image.get(), viewDesc);
    m_Image = std::move(image);
    return 0;
}

Texture* MetalSwapchain::GetImage(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    return m_Image.get();
}

TextureView* MetalSwapchain::GetImageView(uint32_t imageIndex) const
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    return m_ImageView.get();
}

void MetalSwapchain::Present(uint32_t imageIndex)
{
    RTRLAB_ASSERT_MSG(imageIndex == 0, "Early Metal bring-up only exposes a single acquired drawable at a time.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_CurrentDrawable != nil,
                      "Metal presentation requires an acquired drawable.");

    m_Device.PresentDrawable((__bridge void*)m_Data->m_CurrentDrawable);
    m_ImageView.reset();
    m_Image.reset();
    [m_Data->m_CurrentDrawable release];
    m_Data->m_CurrentDrawable = nil;
}

void MetalSwapchain::Resize(uint32_t newWidth, uint32_t newHeight)
{
    if (newWidth == 0 || newHeight == 0)
        return;

    m_Desc.m_Width = newWidth;
    m_Desc.m_Height = newHeight;
    UpdateDrawableSize();
}

void MetalSwapchain::UpdateDrawableSize()
{
    if (m_Data == nullptr || m_Data->m_Layer == nil)
        return;

    m_Data->m_Layer.drawableSize =
        CGSizeMake(static_cast<CGFloat>(m_Desc.m_Width), static_cast<CGFloat>(m_Desc.m_Height));
}

MetalDevice::MetalDevice() : m_Data(new MetalDeviceData())
{
    m_Data->m_Device = MTLCreateSystemDefaultDevice();
    RTRLAB_ASSERT_MSG(m_Data->m_Device != nil, "Failed to create the system Metal device.");

    m_Data->m_CommandQueue = [m_Data->m_Device newCommandQueue];
    RTRLAB_ASSERT_MSG(m_Data->m_CommandQueue != nil, "Failed to create the Metal command queue.");

    m_CommandList.Initialize(m_Data);
}

MetalDevice::~MetalDevice()
{
    m_CommandList.Shutdown();

    if (m_Data != nullptr)
    {
        if (m_Data->m_FrameAutoreleasePool != nil)
        {
            [m_Data->m_FrameAutoreleasePool release];
            m_Data->m_FrameAutoreleasePool = nil;
        }

        if (m_Data->m_CurrentCommandBuffer != nil)
        {
            [m_Data->m_CurrentCommandBuffer release];
            m_Data->m_CurrentCommandBuffer = nil;
        }

        if (m_Data->m_CommandQueue != nil)
        {
            [m_Data->m_CommandQueue release];
            m_Data->m_CommandQueue = nil;
        }

        if (m_Data->m_Device != nil)
        {
            [m_Data->m_Device release];
            m_Data->m_Device = nil;
        }
    }

    delete m_Data;
    m_Data = nullptr;
}

Scope<Swapchain> MetalDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    return CreateScope<MetalSwapchain>(*this, desc, nativeWindowHandle);
}

Scope<Buffer> MetalDevice::CreateBuffer(const BufferDesc& desc)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateBuffer.");

    const NSUInteger size = static_cast<NSUInteger>(std::max<uint64_t>(desc.m_Size, 1));
    const MTLResourceOptions options = ToMetalBufferResourceOptions(desc.m_MemoryUsage);

    id<MTLBuffer> buffer = [m_Data->m_Device newBufferWithLength:size options:options];
    RTRLAB_ASSERT_MSG(buffer != nil, "Failed to create the Metal buffer.");
    SetMetalDebugLabel(buffer, desc.m_DebugName);
    // Metal returns a +1 object here; the wrapper retains for long-lived ownership,
    // and this release balances the factory-created reference.
    auto result = CreateScope<MetalBuffer>(buffer, desc);
    [buffer release];
    return result;
}

Scope<Texture> MetalDevice::CreateTexture(const TextureDesc& desc)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateTexture.");
    // TRANSITIONAL(M3): TextureDesc does not expose a residency / memory-usage
    // policy yet, so v1 Metal textures are always created in Private storage.
    // CPU->GPU uploads will go through staging/upload paths in the next batch,
    // while render-target textures continue to require Private storage. If the
    // public TextureDesc later grows residency controls, this mapping point is
    // where they should feed into Metal storageMode selection.

    MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];
    textureDesc.textureType = ToMetalTextureType(desc.m_Type);
    textureDesc.pixelFormat = ToMetalPixelFormat(desc.m_Format);
    textureDesc.width = std::max(desc.m_Extent.m_Width, 1u);
    textureDesc.height = std::max(desc.m_Extent.m_Height, 1u);
    textureDesc.depth = desc.m_Type == TextureType::Tex3D ? std::max(desc.m_Extent.m_Depth, 1u) : 1u;
    textureDesc.mipmapLevelCount = std::max(desc.m_MipLevels, 1u);
    textureDesc.arrayLength = desc.m_Type == TextureType::Tex2DArray ? std::max(desc.m_ArrayLayers, 1u) : 1u;
    textureDesc.storageMode = MTLStorageModePrivate;
    MTLTextureUsage usage = 0;
    if ((desc.m_UsageMask & TextureUsage::Sampled) != TextureUsage::None)
        usage |= MTLTextureUsageShaderRead;
    if ((desc.m_UsageMask & TextureUsage::Storage) != TextureUsage::None)
        usage |= MTLTextureUsageShaderWrite;
    if ((desc.m_UsageMask & TextureUsage::RenderTarget) != TextureUsage::None ||
        (desc.m_UsageMask & TextureUsage::DepthStencil) != TextureUsage::None)
        usage |= MTLTextureUsageRenderTarget;
    textureDesc.usage = usage != 0 ? usage : MTLTextureUsageUnknown;

    id<MTLTexture> texture = [m_Data->m_Device newTextureWithDescriptor:textureDesc];
    [textureDesc release];
    RTRLAB_ASSERT_MSG(texture != nil, "Failed to create the Metal texture.");
    SetMetalDebugLabel(texture, desc.m_DebugName);
    auto result = CreateScope<MetalTexture>(texture, desc);
    [texture release];
    return result;
}

Scope<TextureView> MetalDevice::CreateTextureView(Texture* texture, const TextureViewDesc& desc)
{
    RTRLAB_ASSERT_MSG(texture != nullptr, "Metal CreateTextureView requires a valid texture.");

    auto* sourceTexture = dynamic_cast<MetalTexture*>(texture);
    RTRLAB_ASSERT_MSG(sourceTexture != nullptr,
                      "Metal CreateTextureView only accepts device-created textures. "
                      "Swapchain images expose views via Swapchain::GetImageView().");

    const TextureDesc& sourceDesc = texture->GetDesc();
    const Format viewFormat = desc.m_Format == Format::Unknown ? sourceDesc.m_Format : desc.m_Format;
    const NSUInteger mipLevelCount =
        desc.m_MipLevelCount == 0 ? std::max(sourceDesc.m_MipLevels - desc.m_BaseMipLevel, 1u) : desc.m_MipLevelCount;
    NSUInteger baseArrayLayer = 0;
    NSUInteger arrayLayerCount = 0;
    if (desc.m_Type == TextureType::Tex3D)
    {
        RTRLAB_ASSERT_MSG(desc.m_BaseArrayLayer == 0 && (desc.m_ArrayLayerCount == 0 || desc.m_ArrayLayerCount == 1),
                          "Metal Tex3D views must have BaseArrayLayer=0 and ArrayLayerCount in {0,1}.");
        baseArrayLayer = 0;
        arrayLayerCount = 1;
    }
    else
    {
        baseArrayLayer = desc.m_BaseArrayLayer;
        arrayLayerCount = desc.m_ArrayLayerCount == 0 ? std::max(sourceDesc.m_ArrayLayers - desc.m_BaseArrayLayer, 1u)
                                                      : desc.m_ArrayLayerCount;
    }

    id<MTLTexture> textureView =
        [sourceTexture->GetMetalTexture() newTextureViewWithPixelFormat:ToMetalPixelFormat(viewFormat)
                                                            textureType:ToMetalTextureType(desc.m_Type)
                                                                 levels:NSMakeRange(desc.m_BaseMipLevel, mipLevelCount)
                                                                 slices:NSMakeRange(baseArrayLayer, arrayLayerCount)];
    RTRLAB_ASSERT_MSG(textureView != nil, "Failed to create the Metal texture view.");
    const std::string debugName = MakeTextureViewDebugName(*texture);
    SetMetalDebugLabel(textureView, debugName.c_str());

    TextureViewDesc resolvedDesc = desc;
    resolvedDesc.m_Format = viewFormat;
    resolvedDesc.m_MipLevelCount = static_cast<uint32_t>(mipLevelCount);
    resolvedDesc.m_BaseArrayLayer = static_cast<uint32_t>(baseArrayLayer);
    resolvedDesc.m_ArrayLayerCount = static_cast<uint32_t>(arrayLayerCount);
    auto result = CreateScope<MetalTextureView>(texture, textureView, resolvedDesc);
    [textureView release];
    return result;
}

Scope<Sampler> MetalDevice::CreateSampler(const SamplerDesc& desc)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateSampler.");

    MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = ToMetalFilter(desc.m_MinFilter);
    samplerDesc.magFilter = ToMetalFilter(desc.m_MagFilter);
    samplerDesc.mipFilter = ToMetalMipFilter(desc.m_MipFilter);
    samplerDesc.sAddressMode = ToMetalAddressMode(desc.m_AddressU);
    samplerDesc.tAddressMode = ToMetalAddressMode(desc.m_AddressV);
    samplerDesc.rAddressMode = ToMetalAddressMode(desc.m_AddressW);
    // TRANSITIONAL(M4): SamplerDesc does not yet expose compare-op or border-color
    // controls, so Metal shadow samplers and explicit border colors are deferred
    // until the shader-system milestone grows the full sampler contract.
    samplerDesc.lodMinClamp = desc.m_MinLod;
    samplerDesc.lodMaxClamp = desc.m_MaxLod;
    samplerDesc.supportArgumentBuffers = NO;
    samplerDesc.maxAnisotropy =
        desc.m_AnisotropyEnable ? static_cast<NSUInteger>(std::max(desc.m_MaxAnisotropy, 1.0f)) : 1;

    id<MTLSamplerState> sampler = [m_Data->m_Device newSamplerStateWithDescriptor:samplerDesc];
    [samplerDesc release];
    RTRLAB_ASSERT_MSG(sampler != nil, "Failed to create the Metal sampler.");
    auto result = CreateScope<MetalSampler>(sampler, desc);
    [sampler release];
    return result;
}

Scope<ShaderProgram> MetalDevice::CreateShaderProgram(const CompiledShaderProgramDesc& desc)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateShaderProgram.");

    std::vector<MetalShaderProgram::StageFunction> functions;
    functions.reserve(desc.m_Blobs.size());

    // TRANSITIONAL(M4): CompiledShaderBlob does not yet carry per-stage entry-point
    // names, so the Metal bring-up path assumes "main_vertex" for vertex stages
    // and "main_fragment" for fragment stages until real shader metadata arrives.
    for (const CompiledShaderBlob& blob : desc.m_Blobs)
    {
        if (blob.m_Backend != BackendType::Metal)
            continue;

        RTRLAB_ASSERT_MSG(blob.m_MetalCodeFormat == MetalCodeFormat::MslSource,
                          "Early Metal shader bring-up currently expects MSL source blobs.");
        NSString* source = MakeNSString(blob.m_Code);
        RTRLAB_ASSERT_MSG(source != nil, "Metal shader blobs must contain valid UTF-8 MSL source.");

        NSError* error = nil;
        id<MTLLibrary> library = [m_Data->m_Device newLibraryWithSource:source options:nil error:&error];
        RTRLAB_ASSERT_MSG(library != nil,
                          error != nil ? [[error localizedDescription] UTF8String]
                                       : "Failed to create the Metal shader library.");

        NSString* entryPoint = nil;
        switch (blob.m_Stage)
        {
            case ShaderStage::Vertex:
                entryPoint = @"main_vertex";
                break;
            case ShaderStage::Fragment:
                entryPoint = @"main_fragment";
                break;
            default:
                [library release];
                RTRLAB_ASSERTF(false, "Unsupported Metal shader stage {}", static_cast<uint32_t>(blob.m_Stage));
                break;
        }

        id<MTLFunction> function = [library newFunctionWithName:entryPoint];
        [library release];
        RTRLAB_ASSERT_MSG(function != nil, "Failed to fetch the requested Metal shader entry point.");
        functions.push_back({blob.m_Stage, function});
    }

    RTRLAB_ASSERT_MSG(!functions.empty(), "Metal CreateShaderProgram requires at least one Metal shader blob.");
    return CreateScope<MetalShaderProgram>(desc, std::move(functions));
}

Scope<VertexInputLayout> MetalDevice::CreateVertexInputLayout(const VertexInputLayoutDesc& desc)
{
    return CreateScope<MetalVertexInputLayout>(desc);
}

Scope<GraphicsPipeline> MetalDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateGraphicsPipeline.");
    RTRLAB_ASSERT_MSG(desc.m_ShaderProgram != nullptr, "Metal graphics pipelines require a ShaderProgram.");
    RTRLAB_ASSERT_MSG(desc.m_VertexInput != nullptr, "Metal graphics pipelines require a VertexInputLayout.");
    RTRLAB_ASSERT_MSG(!desc.m_ColorFormats.empty() || desc.m_DepthFormat != Format::Unknown,
                      "Metal graphics pipelines require at least one render-target format.");

    const MetalShaderProgram& shaderProgram = GetMetalShaderProgram(desc.m_ShaderProgram);
    const MetalVertexInputLayout& vertexInput = GetMetalVertexInputLayout(desc.m_VertexInput);

    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = shaderProgram.FindStage(ShaderStage::Vertex);
    pipelineDesc.fragmentFunction = shaderProgram.FindStage(ShaderStage::Fragment);
    RTRLAB_ASSERT_MSG(pipelineDesc.vertexFunction != nil, "Metal graphics pipelines require a vertex shader stage.");
    RTRLAB_ASSERT_MSG(pipelineDesc.fragmentFunction != nil,
                      "Metal graphics pipelines require a fragment shader stage.");

    MTLVertexDescriptor* vertexDescriptor = [[[MTLVertexDescriptor alloc] init] autorelease];
    const auto& bufferLayouts = vertexInput.GetDesc().m_Buffers;
    for (uint32_t bufferIndex = 0; bufferIndex < static_cast<uint32_t>(bufferLayouts.size()); ++bufferIndex)
    {
        vertexDescriptor.layouts[bufferIndex].stride = bufferLayouts[bufferIndex].m_Stride;
        vertexDescriptor.layouts[bufferIndex].stepFunction = bufferLayouts[bufferIndex].m_PerInstance
                                                                 ? MTLVertexStepFunctionPerInstance
                                                                 : MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[bufferIndex].stepRate = 1;
    }

    for (const VertexAttributeDesc& attribute : vertexInput.GetDesc().m_Attributes)
    {
        RTRLAB_ASSERT_MSG(attribute.m_BufferSlot < bufferLayouts.size(),
                          "Metal graphics pipelines require valid vertex buffer slots.");
        vertexDescriptor.attributes[attribute.m_Location].format = ToMetalVertexFormat(attribute.m_Format);
        vertexDescriptor.attributes[attribute.m_Location].offset = attribute.m_Offset;
        vertexDescriptor.attributes[attribute.m_Location].bufferIndex = attribute.m_BufferSlot;
    }

    pipelineDesc.vertexDescriptor = vertexDescriptor;

    for (uint32_t colorIndex = 0; colorIndex < static_cast<uint32_t>(desc.m_ColorFormats.size()); ++colorIndex)
        pipelineDesc.colorAttachments[colorIndex].pixelFormat = ToMetalPixelFormat(desc.m_ColorFormats[colorIndex]);

    if (desc.m_DepthFormat != Format::Unknown)
        pipelineDesc.depthAttachmentPixelFormat = ToMetalPixelFormat(desc.m_DepthFormat);

    NSError* error = nil;
    id<MTLRenderPipelineState> pipelineState = [m_Data->m_Device newRenderPipelineStateWithDescriptor:pipelineDesc
                                                                                                error:&error];
    [pipelineDesc release];
    RTRLAB_ASSERT_MSG(pipelineState != nil,
                      error != nil ? [[error localizedDescription] UTF8String]
                                   : "Failed to create the Metal render pipeline state.");

    auto result = CreateScope<MetalGraphicsPipeline>(pipelineState, desc);
    [pipelineState release];
    return result;
}

void MetalDevice::WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size)
{
    if (size == 0)
        return;

    RTRLAB_ASSERT_MSG(buffer != nullptr, "Metal WriteBuffer requires a valid buffer.");
    RTRLAB_ASSERT_MSG(data != nullptr, "Metal WriteBuffer requires non-null source data.");

    MetalBuffer& metalBuffer = GetMetalBuffer(buffer);
    RTRLAB_ASSERT_MSG(metalBuffer.GetDesc().m_MemoryUsage == MemoryUsage::CpuToGpu,
                      "Metal WriteBuffer currently requires a CpuToGpu buffer.");
    RTRLAB_ASSERT_MSG(offset + size <= metalBuffer.GetDesc().m_Size,
                      "Metal WriteBuffer range exceeds the buffer size.");

    std::memcpy(
        static_cast<uint8_t*>([metalBuffer.GetMetalBuffer() contents]) + offset, data, static_cast<size_t>(size));
}

CommandList* MetalDevice::BeginCommandList()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_FrameInProgress,
                      "Metal command recording requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameSubmitted, "Metal command recording must happen before submission.");
    return &m_CommandList;
}

void MetalDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "MetalDevice only accepts submissions from its backend command list.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_FrameInProgress, "Metal submission requires an active frame.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameSubmitted,
                      "Metal Submit must only happen once per frame in the current bring-up path.");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(), "Metal Submit requires EndRendering before submission.");

    // Metal keeps the public Submit/present split by deferring commit until Swapchain::present().
    m_Data->m_FrameSubmitted = true;
}

FrameContext* MetalDevice::BeginFrame()
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr, "Metal device data must be initialized before beginning a frame.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameInProgress, "MetalDevice does not support nested frame lifetimes.");
    RTRLAB_ASSERT_MSG(m_Data->m_FrameAutoreleasePool == nil,
                      "Metal frame autorelease pool must be drained before the next frame begins.");

    m_Data->m_FrameAutoreleasePool = [[NSAutoreleasePool alloc] init];

    m_Data->m_CurrentCommandBuffer = [[m_Data->m_CommandQueue commandBuffer] retain];
    RTRLAB_ASSERT_MSG(m_Data->m_CurrentCommandBuffer != nil, "Failed to allocate the Metal command buffer.");

    m_Data->m_FrameInProgress = true;
    m_Data->m_FrameSubmitted = false;
    return &m_FrameContext;
}

void MetalDevice::EndFrame(FrameContext* frameContext)
{
    RTRLAB_ASSERT_MSG(frameContext == &m_FrameContext, "MetalDevice only accepts its backend frame context.");
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_FrameInProgress, "Metal EndFrame requires an active frame.");
    RTRLAB_ASSERT_MSG(m_Data->m_FrameSubmitted, "Metal EndFrame currently expects submission before the frame closes.");

    m_Data->m_FrameInProgress = false;
}

void MetalDevice::PresentDrawable(void* drawable)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr, "Metal device data must remain valid until presentation.");
    RTRLAB_ASSERT_MSG(!m_Data->m_FrameInProgress, "Metal drawable presentation requires EndFrame first.");
    RTRLAB_ASSERT_MSG(m_Data->m_FrameSubmitted, "Metal drawable presentation requires a submitted frame.");
    RTRLAB_ASSERT_MSG(m_Data->m_CurrentCommandBuffer != nil,
                      "Metal drawable presentation requires an active command buffer.");

    id<CAMetalDrawable> metalDrawable = (__bridge id<CAMetalDrawable>)drawable;
    RTRLAB_ASSERT_MSG(metalDrawable != nil, "Metal drawable presentation requires a valid CAMetalDrawable.");

    [m_Data->m_CurrentCommandBuffer presentDrawable:metalDrawable];
    [m_Data->m_CurrentCommandBuffer commit];
    [m_Data->m_CurrentCommandBuffer release];
    m_Data->m_CurrentCommandBuffer = nil;

    if (m_Data->m_FrameAutoreleasePool != nil)
    {
        [m_Data->m_FrameAutoreleasePool release];
        m_Data->m_FrameAutoreleasePool = nil;
    }

    m_Data->m_FrameSubmitted = false;
}
