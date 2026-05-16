#include "Render/RHI/Backends/Metal/Device/MetalDevice.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Metal/Resources/MetalBuffer.h"
#include "Render/RHI/Backends/Metal/Common/MetalCommon.h"
#include "Render/RHI/Backends/Metal/Common/MetalConversions.h"
#include "Render/RHI/Backends/Metal/Pipeline/MetalGraphicsPipeline.h"
#include "Render/RHI/Backends/Metal/Pipeline/MetalPipelineLayout.h"
#include "Render/RHI/Backends/Metal/Resources/MetalResourceSet.h"
#include "Render/RHI/Backends/Metal/Resources/MetalSampler.h"
#include "Render/RHI/Backends/Metal/Pipeline/MetalShaderProgram.h"
#include "Render/RHI/Backends/Metal/Presentation/MetalSwapchain.h"
#include "Render/RHI/Backends/Metal/Resources/MetalTexture.h"

using namespace MetalRHI;

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
    const BufferDesc sanitizedDesc = RHIInternal::SanitizeBufferDesc(desc);

    const NSUInteger size = static_cast<NSUInteger>(sanitizedDesc.m_Size);
    const MTLResourceOptions options = ToMetalBufferResourceOptions(sanitizedDesc.m_MemoryUsage);

    id<MTLBuffer> buffer = [m_Data->m_Device newBufferWithLength:size options:options];
    RTRLAB_ASSERT_MSG(buffer != nil, "Failed to create the Metal buffer.");
    SetMetalDebugLabel(buffer, sanitizedDesc.m_DebugName);
    // Metal returns a +1 object here; the wrapper retains for long-lived ownership,
    // and this release balances the factory-created reference.
    auto result = CreateScope<MetalBuffer>(buffer, sanitizedDesc);
    [buffer release];
    return result;
}

Scope<Texture> MetalDevice::CreateTexture(const TextureDesc& desc)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateTexture.");
    const TextureDesc sanitizedDesc = RHIInternal::SanitizeTextureDesc(desc);
    // TRANSITIONAL(M3): TextureDesc does not expose a residency / memory-usage
    // policy yet, so v1 Metal textures are always created in Private storage.
    // CPU->GPU uploads will go through staging/upload paths in the next batch,
    // while render-target textures continue to require Private storage. If the
    // public TextureDesc later grows residency controls, this mapping point is
    // where they should feed into Metal storageMode selection.

    MTLTextureDescriptor* textureDesc = [[MTLTextureDescriptor alloc] init];
    textureDesc.textureType = ToMetalTextureType(sanitizedDesc.m_Type);
    textureDesc.pixelFormat = ToMetalPixelFormat(sanitizedDesc.m_Format);
    textureDesc.width = sanitizedDesc.m_Extent.m_Width;
    textureDesc.height = sanitizedDesc.m_Extent.m_Height;
    textureDesc.depth = sanitizedDesc.m_Type == TextureType::Tex3D ? sanitizedDesc.m_Extent.m_Depth : 1u;
    textureDesc.mipmapLevelCount = sanitizedDesc.m_MipLevels;
    textureDesc.arrayLength =
        sanitizedDesc.m_Type == TextureType::Tex2DArray || sanitizedDesc.m_Type == TextureType::Cube
            ? sanitizedDesc.m_ArrayLayers
            : 1u;
    textureDesc.storageMode = MTLStorageModePrivate;
    MTLTextureUsage usage = 0;
    if ((sanitizedDesc.m_UsageMask & TextureUsage::Sampled) != TextureUsage::None)
        usage |= MTLTextureUsageShaderRead;
    if ((sanitizedDesc.m_UsageMask & TextureUsage::Storage) != TextureUsage::None)
        usage |= MTLTextureUsageShaderWrite;
    if ((sanitizedDesc.m_UsageMask & TextureUsage::RenderTarget) != TextureUsage::None ||
        (sanitizedDesc.m_UsageMask & TextureUsage::DepthStencil) != TextureUsage::None)
        usage |= MTLTextureUsageRenderTarget;
    textureDesc.usage = usage != 0 ? usage : MTLTextureUsageUnknown;

    id<MTLTexture> texture = [m_Data->m_Device newTextureWithDescriptor:textureDesc];
    [textureDesc release];
    RTRLAB_ASSERT_MSG(texture != nil, "Failed to create the Metal texture.");
    SetMetalDebugLabel(texture, sanitizedDesc.m_DebugName);
    auto result = CreateScope<MetalTexture>(texture, sanitizedDesc);
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
    samplerDesc.supportArgumentBuffers = YES;
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
    std::vector<MetalSetBindingPlan> usedSetBindingPlans;

    for (const CompiledShaderBlob& blob : desc.m_Blobs)
    {
        if (blob.m_Backend != BackendType::Metal)
            continue;

        RTRLAB_ASSERT_MSG(blob.m_MetalCodeFormat == MetalCodeFormat::MslSource,
                          "Early Metal shader bring-up currently expects MSL source blobs.");
        std::vector<MetalSetBindingPlan> blobUsedSetPlans;
        const std::string remappedSource = RewriteMetalShaderSourceForArgumentBuffers(
            desc,
            blob,
            std::string_view(reinterpret_cast<const char*>(blob.m_Code.data()), blob.m_Code.size()),
            &blobUsedSetPlans);
        MergeUsedMetalSetBindingPlans(&usedSetBindingPlans, std::move(blobUsedSetPlans));
        NSString* source = MakeNSString(std::vector<uint8_t>(remappedSource.begin(), remappedSource.end()));
        RTRLAB_ASSERT_MSG(source != nil, "Metal shader blobs must contain valid UTF-8 MSL source.");

        NSError* error = nil;
        id<MTLLibrary> library = [m_Data->m_Device newLibraryWithSource:source options:nil error:&error];
        RTRLAB_ASSERT_MSG(library != nil,
                          error != nil ? [[error localizedDescription] UTF8String]
                                       : "Failed to create the Metal shader library.");

        NSString* entryPoint = MakeNSString(blob.m_EntryPoint.c_str());
        if (entryPoint == nil)
        {
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
        }

        id<MTLFunction> function = [library newFunctionWithName:entryPoint];
        [library release];
        RTRLAB_ASSERT_MSG(function != nil, "Failed to fetch the requested Metal shader entry point.");
        functions.push_back({blob.m_Stage, function});
    }

    RTRLAB_ASSERT_MSG(!functions.empty(), "Metal CreateShaderProgram requires at least one Metal shader blob.");
    return CreateScope<MetalShaderProgram>(desc, std::move(functions), std::move(usedSetBindingPlans));
}

Scope<PipelineLayout> MetalDevice::CreatePipelineLayout(const PipelineLayoutDesc& desc)
{
    RHIInternal::ValidatePipelineLayoutDesc(desc);
    return CreateScope<MetalPipelineLayout>(desc);
}

Scope<ResourceSet> MetalDevice::CreateResourceSet(PipelineLayout* layout, uint32_t setIndex)
{
    RTRLAB_ASSERT_MSG(m_Data != nullptr && m_Data->m_Device != nil,
                      "Metal device must be initialized before CreateResourceSet.");
    RHIInternal::ValidateResourceSetDesc(layout, setIndex);
    return CreateScope<MetalResourceSet>(m_Data->m_Device, layout, setIndex);
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
    RTRLAB_ASSERT_MSG(desc.m_PipelineLayout != nullptr, "Metal graphics pipelines require a PipelineLayout.");
    RTRLAB_ASSERT_MSG(desc.m_VertexInput != nullptr, "Metal graphics pipelines require a VertexInputLayout.");
    RTRLAB_ASSERT_MSG(!desc.m_ColorFormats.empty() || desc.m_DepthFormat != Format::Unknown,
                      "Metal graphics pipelines require at least one render-target format.");

    const MetalShaderProgram& shaderProgram = GetMetalShaderProgram(desc.m_ShaderProgram);
    const MetalVertexInputLayout& vertexInput = GetMetalVertexInputLayout(desc.m_VertexInput);
    const MetalPipelineLayout& pipelineLayout = GetMetalPipelineLayout(desc.m_PipelineLayout);

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
        const uint32_t metalBufferIndex = pipelineLayout.GetVertexBufferSlotBase() + bufferIndex;
        vertexDescriptor.layouts[metalBufferIndex].stride = bufferLayouts[bufferIndex].m_Stride;
        vertexDescriptor.layouts[metalBufferIndex].stepFunction = bufferLayouts[bufferIndex].m_PerInstance
                                                                      ? MTLVertexStepFunctionPerInstance
                                                                      : MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[metalBufferIndex].stepRate = 1;
    }

    for (const VertexAttributeDesc& attribute : vertexInput.GetDesc().m_Attributes)
    {
        RTRLAB_ASSERT_MSG(attribute.m_BufferSlot < bufferLayouts.size(),
                          "Metal graphics pipelines require valid vertex buffer slots.");
        vertexDescriptor.attributes[attribute.m_Location].format = ToMetalVertexFormat(attribute.m_Format);
        vertexDescriptor.attributes[attribute.m_Location].offset = attribute.m_Offset;
        vertexDescriptor.attributes[attribute.m_Location].bufferIndex =
            pipelineLayout.GetVertexBufferSlotBase() + attribute.m_BufferSlot;
    }

    pipelineDesc.vertexDescriptor = vertexDescriptor;

    for (uint32_t colorIndex = 0; colorIndex < static_cast<uint32_t>(desc.m_ColorFormats.size()); ++colorIndex)
        pipelineDesc.colorAttachments[colorIndex].pixelFormat = ToMetalPixelFormat(desc.m_ColorFormats[colorIndex]);

    if (desc.m_DepthFormat != Format::Unknown)
    {
        pipelineDesc.depthAttachmentPixelFormat = ToMetalPixelFormat(desc.m_DepthFormat);
        if (HasStencilComponent(desc.m_DepthFormat))
            pipelineDesc.stencilAttachmentPixelFormat = ToMetalPixelFormat(desc.m_DepthFormat);
    }

    id<MTLDepthStencilState> depthStencilState = nil;
    if (desc.m_DepthFormat != Format::Unknown)
    {
        MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthStencilDesc.depthCompareFunction = ToMetalCompareFunction(desc.m_DepthStencilState.m_DepthCompareOp);
        depthStencilDesc.depthWriteEnabled = desc.m_DepthStencilState.m_DepthWriteEnable ? YES : NO;
        depthStencilState = [m_Data->m_Device newDepthStencilStateWithDescriptor:depthStencilDesc];
        [depthStencilDesc release];
        RTRLAB_ASSERT_MSG(depthStencilState != nil, "Failed to create the Metal depth-stencil state.");
    }

    std::vector<MetalStageArgumentEncoderEntry> argumentEncoders;
    for (const MetalSetBindingPlan& setPlan : shaderProgram.GetSetBindingPlans())
    {
        for (const MetalStageBindingPlan& stagePlan : setPlan.m_StagePlans)
        {
            id<MTLFunction> stageFunction = nil;
            switch (stagePlan.m_Stage)
            {
                case ShaderStage::Vertex:
                    stageFunction = pipelineDesc.vertexFunction;
                    break;
                case ShaderStage::Fragment:
                    stageFunction = pipelineDesc.fragmentFunction;
                    break;
                default:
                    break;
            }

            if (stageFunction == nil)
                continue;

            id<MTLArgumentEncoder> argumentEncoder =
                [stageFunction newArgumentEncoderWithBufferIndex:stagePlan.m_ArgumentBufferSlot];
            RTRLAB_ASSERT_MSG(argumentEncoder != nil,
                              "Failed to create a Metal argument encoder for a resource set binding.");
            argumentEncoders.push_back(
                {setPlan.m_SetIndex, stagePlan.m_Stage, stagePlan.m_ArgumentBufferSlot, argumentEncoder});
        }
    }

    NSError* error = nil;
    id<MTLRenderPipelineState> pipelineState = [m_Data->m_Device newRenderPipelineStateWithDescriptor:pipelineDesc
                                                                                                error:&error];
    [pipelineDesc release];
    RTRLAB_ASSERT_MSG(pipelineState != nil,
                      error != nil ? [[error localizedDescription] UTF8String]
                                   : "Failed to create the Metal render pipeline state.");

    auto result = CreateScope<MetalGraphicsPipeline>(
        pipelineState, depthStencilState, desc, pipelineLayout.GetVertexBufferSlotBase(), std::move(argumentEncoders));
    [pipelineState release];
    if (depthStencilState != nil)
        [depthStencilState release];
    return result;
}

Scope<ComputePipeline> MetalDevice::CreateComputePipeline(const ComputePipelineDesc&)
{
    RTRLAB_ASSERT_MSG(false,
                      "Metal compute pipelines are not implemented yet. This backend no longer creates shell "
                      "compute-pipeline placeholders.");
    return nullptr;
}

void MetalDevice::WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size)
{
    // TRANSITIONAL(M3): Demo-only direct host upload path for early bring-up.
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
    m_CommandList.ResetState();
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
