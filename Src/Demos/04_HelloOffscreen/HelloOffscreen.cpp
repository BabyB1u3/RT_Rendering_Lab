#include "HelloOffscreen.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Util/Math.h"
#include "Render/Shader/ShaderCompiler.h"
#include "Render/Shader/ShaderParameterWriter.h"

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
namespace HelloOffscreenDemo
{
struct TexturedVertex
{
    Math::Vec2 m_Position;
    Math::Vec2 m_UV;
};

struct DemoViewport
{
    float m_X = 0.0f;
    float m_Y = 0.0f;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
};

DemoViewport ComputeAspectPreservingViewport(uint32_t framebufferWidth, uint32_t framebufferHeight)
{
    RTRLAB_ASSERT_MSG(framebufferWidth > 0 && framebufferHeight > 0,
                      "HelloOffscreen requires a non-zero framebuffer size.");

    const float squareExtent = static_cast<float>(std::min(framebufferWidth, framebufferHeight));

    DemoViewport viewport;
    viewport.m_Width = squareExtent;
    viewport.m_Height = squareExtent;
    viewport.m_X = (static_cast<float>(framebufferWidth) - squareExtent) * 0.5f;
    viewport.m_Y = (static_cast<float>(framebufferHeight) - squareExtent) * 0.5f;
    return viewport;
}

uint32_t FindRequiredSetIndex(const PipelineLayoutDesc& layoutDesc, std::string_view bindingName)
{
    const auto it = std::find_if(layoutDesc.m_Bindings.begin(),
                                 layoutDesc.m_Bindings.end(),
                                 [bindingName](const BindingInfo& binding) { return binding.m_Name == bindingName; });
    RTRLAB_ASSERTF(it != layoutDesc.m_Bindings.end(),
                   "HelloOffscreen failed to find reflected binding '{}' in the PipelineLayout.",
                   bindingName);
    return it->m_SetIndex;
}

ShaderCompileRequest BuildOffscreenPassShaderCompileRequest()
{
    ShaderCompileRequest request;

    const std::filesystem::path shaderPath =
        FileSystem::GetRootPath() / "Project" / "Shaders" / "HelloOffscreenPass.slang";
    const std::string shaderModule = shaderPath.generic_string();

#if defined(GLAB_BACKEND_VULKAN)
    request.m_Targets.push_back({BackendType::Vulkan, MetalCodeFormat::MslSource});
#elif defined(GLAB_BACKEND_METAL)
    request.m_Targets.push_back({BackendType::Metal, MetalCodeFormat::MslSource});
#endif

    request.m_Source.m_Entries.push_back({shaderModule, "main_vertex", ShaderStage::Vertex});
    request.m_Source.m_Entries.push_back({shaderModule, "main_fragment", ShaderStage::Fragment});
    return request;
}

ShaderCompileRequest BuildOffscreenCompositeShaderCompileRequest()
{
    ShaderCompileRequest request;

    const std::filesystem::path shaderPath =
        FileSystem::GetRootPath() / "Project" / "Shaders" / "HelloOffscreenComposite.slang";
    const std::string shaderModule = shaderPath.generic_string();

#if defined(GLAB_BACKEND_VULKAN)
    request.m_Targets.push_back({BackendType::Vulkan, MetalCodeFormat::MslSource});
#elif defined(GLAB_BACKEND_METAL)
    request.m_Targets.push_back({BackendType::Metal, MetalCodeFormat::MslSource});
#endif

    request.m_Source.m_Entries.push_back({shaderModule, "main_vertex", ShaderStage::Vertex});
    request.m_Source.m_Entries.push_back({shaderModule, "main_fragment", ShaderStage::Fragment});
    return request;
}

CompiledShaderProgramDesc CompileShaderProgramDesc(const ShaderCompileRequest& request, const char* debugLabel)
{
    Scope<ShaderCompiler> shaderCompiler = CreateShaderCompiler();
    RTRLAB_ASSERT_MSG(shaderCompiler != nullptr, "HelloOffscreen requires a valid ShaderCompiler instance.");

    ShaderCompileResult compileResult = shaderCompiler->CompileProgram(request);
    RTRLAB_ASSERTF(compileResult.m_Succeeded,
                   "HelloOffscreen failed to compile {} shader program: {}",
                   debugLabel,
                   compileResult.m_ErrorMessage);
    return std::move(compileResult.m_Program);
}
} // namespace HelloOffscreenDemo
#endif

HelloOffscreen::HelloOffscreen(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height) {}

void HelloOffscreen::OnAttach()
{
    CreateOffscreenDemoResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloOffscreen demo attached");
}

void HelloOffscreen::OnDetach()
{
    ForgetTrackedResources();
    m_CompositePipeline.reset();
    m_CompositeTextureSet.reset();
    m_CompositePipelineLayout.reset();
    m_CompositeShaderProgram.reset();
    m_OffscreenPipeline.reset();
    m_OffscreenPipelineLayout.reset();
    m_OffscreenShaderProgram.reset();
    m_VertexInputLayout.reset();
    m_OffscreenSampler.reset();
    m_OffscreenColorView.reset();
    m_OffscreenColorTexture.reset();
    m_IndexUploadBuffer.reset();
    m_VertexUploadBuffer.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
    m_GeometryUploadPending = false;
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloOffscreen demo detached");
}

void HelloOffscreen::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_OffscreenPipeline || !m_CompositePipeline || !m_VertexBuffer || !m_IndexBuffer || !m_CompositeTextureSet)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "HelloOffscreen requires an active command list during OnRender.");
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "HelloOffscreen requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadPendingGeometry(*commandList, resourceStateTracker);

    MeshBinding meshBinding;
    meshBinding.m_VertexBuffers = {m_VertexBuffer.get()};
    meshBinding.m_IndexBuffer = m_IndexBuffer.get();
    meshBinding.m_IndexType = IndexType::UInt16;

    resourceStateTracker.Transition(m_OffscreenColorTexture.get(), TextureState::RenderTarget);
    resourceStateTracker.FlushBarriers(commandList);

    const TextureDesc& offscreenDesc = m_OffscreenColorTexture->GetDesc();
    ColorAttachmentInfo offscreenColorAttachment;
    offscreenColorAttachment.m_View = m_OffscreenColorView.get();
    offscreenColorAttachment.m_LoadOp = LoadOp::Clear;
    offscreenColorAttachment.m_StoreOp = StoreOp::Store;
    offscreenColorAttachment.m_ClearValue = {0.02f, 0.03f, 0.05f, 1.0f};

    RenderingInfo offscreenRenderingInfo;
    offscreenRenderingInfo.m_ColorAttachments = {offscreenColorAttachment};
    offscreenRenderingInfo.m_RenderArea = {0, 0, offscreenDesc.m_Extent.m_Width, offscreenDesc.m_Extent.m_Height};
    commandList->BeginRendering(offscreenRenderingInfo);
    commandList->SetViewport(0.0f,
                             0.0f,
                             static_cast<float>(offscreenDesc.m_Extent.m_Width),
                             static_cast<float>(offscreenDesc.m_Extent.m_Height),
                             0.0f,
                             1.0f);
    commandList->SetScissor(0, 0, offscreenDesc.m_Extent.m_Width, offscreenDesc.m_Extent.m_Height);
    commandList->BindGraphicsPipeline(m_OffscreenPipeline.get());
    commandList->BindMesh(meshBinding);
    commandList->DrawIndexed(6, 0, 0);
    commandList->EndRendering();

    resourceStateTracker.Transition(m_OffscreenColorTexture.get(), TextureState::ShaderRead);
    resourceStateTracker.Transition(swapchainImage, TextureState::RenderTarget);
    resourceStateTracker.FlushBarriers(commandList);

    ColorAttachmentInfo swapchainColorAttachment;
    swapchainColorAttachment.m_View = swapchainImageView;
    swapchainColorAttachment.m_LoadOp = LoadOp::Clear;
    swapchainColorAttachment.m_StoreOp = StoreOp::Store;
    swapchainColorAttachment.m_ClearValue = {0.08f, 0.10f, 0.12f, 1.0f};

    RenderingInfo swapchainRenderingInfo;
    swapchainRenderingInfo.m_ColorAttachments = {swapchainColorAttachment};
    swapchainRenderingInfo.m_RenderArea = {0, 0, m_ViewportWidth, m_ViewportHeight};
    commandList->BeginRendering(swapchainRenderingInfo);

    const HelloOffscreenDemo::DemoViewport viewport =
        HelloOffscreenDemo::ComputeAspectPreservingViewport(m_ViewportWidth, m_ViewportHeight);
    commandList->SetViewport(viewport.m_X, viewport.m_Y, viewport.m_Width, viewport.m_Height, 0.0f, 1.0f);
    commandList->SetScissor(0, 0, m_ViewportWidth, m_ViewportHeight);
    commandList->BindGraphicsPipeline(m_CompositePipeline.get());
    commandList->BindResourceSet(m_CompositeTextureSetIndex, m_CompositeTextureSet.get());
    commandList->BindMesh(meshBinding);
    commandList->DrawIndexed(6, 0, 0);
    commandList->EndRendering();
#endif
}

void HelloOffscreen::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

void HelloOffscreen::CreateOffscreenDemoResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    static const std::array<HelloOffscreenDemo::TexturedVertex, 4> kVertices = {{
        {{-1.0f, -1.0f}, {0.0f, 1.0f}},
        {{1.0f, -1.0f}, {1.0f, 1.0f}},
        {{1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-1.0f, 1.0f}, {0.0f, 0.0f}},
    }};
    static constexpr std::array<uint16_t, 6> kIndices = {0, 1, 2, 0, 2, 3};

    BufferDesc vertexBufferDesc;
    vertexBufferDesc.m_Size = sizeof(kVertices);
    vertexBufferDesc.m_UsageMask = BufferUsage::Vertex | BufferUsage::CopyDst;
    vertexBufferDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    vertexBufferDesc.m_DebugName = "HelloOffscreen.VertexBuffer";
    m_VertexBuffer = device.CreateBuffer(vertexBufferDesc);

    BufferDesc vertexUploadBufferDesc;
    vertexUploadBufferDesc.m_Size = sizeof(kVertices);
    vertexUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    vertexUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    vertexUploadBufferDesc.m_DebugName = "HelloOffscreen.VertexUploadBuffer";
    m_VertexUploadBuffer = device.CreateBuffer(vertexUploadBufferDesc);
    device.WriteBuffer(m_VertexUploadBuffer.get(), 0, kVertices.data(), sizeof(kVertices));

    BufferDesc indexBufferDesc;
    indexBufferDesc.m_Size = sizeof(kIndices);
    indexBufferDesc.m_UsageMask = BufferUsage::Index | BufferUsage::CopyDst;
    indexBufferDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    indexBufferDesc.m_DebugName = "HelloOffscreen.IndexBuffer";
    m_IndexBuffer = device.CreateBuffer(indexBufferDesc);

    BufferDesc indexUploadBufferDesc;
    indexUploadBufferDesc.m_Size = sizeof(kIndices);
    indexUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    indexUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    indexUploadBufferDesc.m_DebugName = "HelloOffscreen.IndexUploadBuffer";
    m_IndexUploadBuffer = device.CreateBuffer(indexUploadBufferDesc);
    device.WriteBuffer(m_IndexUploadBuffer.get(), 0, kIndices.data(), sizeof(kIndices));
    m_GeometryUploadPending = true;

    TextureDesc offscreenTextureDesc;
    offscreenTextureDesc.m_Type = TextureType::Tex2D;
    offscreenTextureDesc.m_Format = Format::RGBA8_UNORM;
    offscreenTextureDesc.m_Extent = {512, 512, 1};
    offscreenTextureDesc.m_MipLevels = 1;
    offscreenTextureDesc.m_ArrayLayers = 1;
    offscreenTextureDesc.m_UsageMask = TextureUsage::RenderTarget | TextureUsage::Sampled;
    offscreenTextureDesc.m_DebugName = "HelloOffscreen.ColorTarget";
    m_OffscreenColorTexture = device.CreateTexture(offscreenTextureDesc);

    TextureViewDesc offscreenViewDesc;
    offscreenViewDesc.m_Type = TextureType::Tex2D;
    offscreenViewDesc.m_Format = offscreenTextureDesc.m_Format;
    offscreenViewDesc.m_Aspect = TextureAspect::Color;
    offscreenViewDesc.m_BaseMipLevel = 0;
    offscreenViewDesc.m_MipLevelCount = 1;
    offscreenViewDesc.m_BaseArrayLayer = 0;
    offscreenViewDesc.m_ArrayLayerCount = 1;
    m_OffscreenColorView = device.CreateTextureView(m_OffscreenColorTexture.get(), offscreenViewDesc);

    SamplerDesc samplerDesc;
    samplerDesc.m_MinFilter = FilterMode::Linear;
    samplerDesc.m_MagFilter = FilterMode::Linear;
    samplerDesc.m_MipFilter = MipFilterMode::Linear;
    samplerDesc.m_AddressU = AddressMode::ClampToEdge;
    samplerDesc.m_AddressV = AddressMode::ClampToEdge;
    samplerDesc.m_AddressW = AddressMode::ClampToEdge;
    m_OffscreenSampler = device.CreateSampler(samplerDesc);

    m_OffscreenShaderProgram = device.CreateShaderProgram(HelloOffscreenDemo::CompileShaderProgramDesc(
        HelloOffscreenDemo::BuildOffscreenPassShaderCompileRequest(), "offscreen pass"));
    m_OffscreenPipelineLayout = device.CreatePipelineLayout(m_OffscreenShaderProgram->DerivePipelineLayoutDesc());

    m_CompositeShaderProgram = device.CreateShaderProgram(HelloOffscreenDemo::CompileShaderProgramDesc(
        HelloOffscreenDemo::BuildOffscreenCompositeShaderCompileRequest(), "composite"));
    m_CompositePipelineLayout = device.CreatePipelineLayout(m_CompositeShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& compositeLayoutDesc = m_CompositePipelineLayout->GetDesc();
    m_CompositeTextureSetIndex = HelloOffscreenDemo::FindRequiredSetIndex(compositeLayoutDesc, "gOffscreenTexture");
    m_CompositeTextureSet = device.CreateResourceSet(m_CompositePipelineLayout.get(), m_CompositeTextureSetIndex);

    ShaderParameterWriter compositeParameterWriter(m_CompositeShaderProgram->GetReflection());
    compositeParameterWriter.SetTextureView(*m_CompositeTextureSet, "gOffscreenTexture", m_OffscreenColorView.get());
    compositeParameterWriter.SetSampler(*m_CompositeTextureSet, "gLinearSampler", m_OffscreenSampler.get());

    VertexInputLayoutDesc vertexInputLayoutDesc;
    vertexInputLayoutDesc.m_Buffers = {{static_cast<uint32_t>(sizeof(HelloOffscreenDemo::TexturedVertex)), false}};
    vertexInputLayoutDesc.m_Attributes = {
        {0u, Format::RG32F, static_cast<uint32_t>(offsetof(HelloOffscreenDemo::TexturedVertex, m_Position)), 0u},
        {1u, Format::RG32F, static_cast<uint32_t>(offsetof(HelloOffscreenDemo::TexturedVertex, m_UV)), 0u},
    };
    m_VertexInputLayout = device.CreateVertexInputLayout(vertexInputLayoutDesc);

    GraphicsPipelineDesc offscreenPipelineDesc;
    offscreenPipelineDesc.m_PipelineLayout = m_OffscreenPipelineLayout.get();
    offscreenPipelineDesc.m_ShaderProgram = m_OffscreenShaderProgram.get();
    offscreenPipelineDesc.m_VertexInput = m_VertexInputLayout.get();
    offscreenPipelineDesc.m_RasterState.m_CullMode = CullMode::None;
    offscreenPipelineDesc.m_ColorFormats = {offscreenTextureDesc.m_Format};
    m_OffscreenPipeline = device.CreateGraphicsPipeline(offscreenPipelineDesc);

    GraphicsPipelineDesc compositePipelineDesc;
    compositePipelineDesc.m_PipelineLayout = m_CompositePipelineLayout.get();
    compositePipelineDesc.m_ShaderProgram = m_CompositeShaderProgram.get();
    compositePipelineDesc.m_VertexInput = m_VertexInputLayout.get();
    compositePipelineDesc.m_RasterState.m_CullMode = CullMode::None;
    compositePipelineDesc.m_ColorFormats = {app.GetSwapchain().GetFormat()};
    m_CompositePipeline = device.CreateGraphicsPipeline(compositePipelineDesc);
#endif
}

void HelloOffscreen::UploadPendingGeometry(CommandList& commandList, ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GeometryUploadPending)
        return;

    RTRLAB_ASSERT_MSG(m_VertexUploadBuffer != nullptr && m_VertexBuffer != nullptr,
                      "HelloOffscreen geometry upload requires both vertex staging and GPU buffers.");
    RTRLAB_ASSERT_MSG(m_IndexUploadBuffer != nullptr && m_IndexBuffer != nullptr,
                      "HelloOffscreen geometry upload requires both index staging and GPU buffers.");

    resourceStateTracker.Transition(m_VertexUploadBuffer.get(), BufferState::CopySource);
    resourceStateTracker.Transition(m_VertexBuffer.get(), BufferState::CopyDest);
    resourceStateTracker.Transition(m_IndexUploadBuffer.get(), BufferState::CopySource);
    resourceStateTracker.Transition(m_IndexBuffer.get(), BufferState::CopyDest);
    resourceStateTracker.FlushBarriers(&commandList);

    const std::array<BufferCopyRegion, 1> vertexCopyRegions = {{
        {0, 0, m_VertexBuffer->GetDesc().m_Size},
    }};
    commandList.CopyBuffer(m_VertexUploadBuffer.get(), m_VertexBuffer.get(), vertexCopyRegions);

    const std::array<BufferCopyRegion, 1> indexCopyRegions = {{
        {0, 0, m_IndexBuffer->GetDesc().m_Size},
    }};
    commandList.CopyBuffer(m_IndexUploadBuffer.get(), m_IndexBuffer.get(), indexCopyRegions);

    resourceStateTracker.Transition(m_VertexBuffer.get(), BufferState::VertexIndex);
    resourceStateTracker.Transition(m_IndexBuffer.get(), BufferState::VertexIndex);
    resourceStateTracker.FlushBarriers(&commandList);

    m_GeometryUploadPending = false;
#endif
}

void HelloOffscreen::ForgetTrackedResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_OffscreenColorTexture.get());
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}
