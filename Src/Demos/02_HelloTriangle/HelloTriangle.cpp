#include "HelloTriangle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Util/Math.h"
#include "Core/Util/Time.h"
#include "Render/Shader/ShaderCompiler.h"
#include "Render/Shader/ShaderParameterWriter.h"

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
namespace
{
struct TriangleVertex
{
    Math::Vec2 m_Position;
    Math::Vec4 m_Color;
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
                      "HelloTriangle requires a non-zero framebuffer size.");

    // Keep the demo presentation square so the simple parameterized transform
    // still looks centered and consistent across window aspect ratios.
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
                   "HelloTriangle failed to find reflected binding '{}' in the PipelineLayout.",
                   bindingName);
    return it->m_SetIndex;
}

ShaderCompileRequest BuildHelloTriangleShaderCompileRequest()
{
    ShaderCompileRequest request;

    const std::filesystem::path shaderPath = FileSystem::GetRootPath() / "Project" / "Shaders" / "HelloTriangle.slang";
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

CompiledShaderProgramDesc BuildHelloTriangleShaderProgramDesc()
{
    Scope<ShaderCompiler> shaderCompiler = CreateShaderCompiler();
    RTRLAB_ASSERT_MSG(shaderCompiler != nullptr, "HelloTriangle requires a valid ShaderCompiler instance.");

    ShaderCompileResult compileResult = shaderCompiler->CompileProgram(BuildHelloTriangleShaderCompileRequest());
    RTRLAB_ASSERTF(compileResult.m_Succeeded,
                   "HelloTriangle failed to compile its Slang shader program: {}",
                   compileResult.m_ErrorMessage);
    return std::move(compileResult.m_Program);
}
} // namespace
#endif

HelloTriangle::HelloTriangle(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height) {}

void HelloTriangle::OnAttach()
{
    CreateTriangleResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloTriangle demo attached");
}

void HelloTriangle::OnDetach()
{
    ForgetTrackedBuffers();
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_ObjectSet.reset();
    m_MaterialSet.reset();
    m_FrameSet.reset();
    m_PipelineLayout.reset();
    m_ShaderProgram.reset();
    m_IndexUploadBuffer.reset();
    m_VertexUploadBuffer.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
    m_GeometryUploadPending = false;
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloTriangle demo detached");
}

void HelloTriangle::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "HelloTriangle requires an active command list during OnRender.");
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "HelloTriangle requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadTriangleGeometry(*commandList, resourceStateTracker);

    resourceStateTracker.Transition(swapchainImage, TextureState::RenderTarget);
    resourceStateTracker.FlushBarriers(commandList);

    ColorAttachmentInfo colorAttachment;
    colorAttachment.m_View = swapchainImageView;
    colorAttachment.m_LoadOp = LoadOp::Clear;
    colorAttachment.m_StoreOp = StoreOp::Store;
    colorAttachment.m_ClearValue = {0.08f, 0.10f, 0.12f, 1.0f};

    RenderingInfo renderingInfo;
    renderingInfo.m_ColorAttachments = {colorAttachment};
    renderingInfo.m_RenderArea = {0, 0, m_ViewportWidth, m_ViewportHeight};
    commandList->BeginRendering(renderingInfo);

    UpdateAnimatedParameters();

    MeshBinding meshBinding;
    meshBinding.m_VertexBuffers = {m_VertexBuffer.get()};
    meshBinding.m_IndexBuffer = m_IndexBuffer.get();
    meshBinding.m_IndexType = IndexType::UInt16;

    const DemoViewport viewport = ComputeAspectPreservingViewport(m_ViewportWidth, m_ViewportHeight);
    commandList->SetViewport(viewport.m_X, viewport.m_Y, viewport.m_Width, viewport.m_Height, 0.0f, 1.0f);
    commandList->SetScissor(0, 0, m_ViewportWidth, m_ViewportHeight);
    commandList->BindGraphicsPipeline(m_GraphicsPipeline.get());
    commandList->BindResourceSet(m_FrameSetIndex, m_FrameSet.get());
    commandList->BindResourceSet(m_MaterialSetIndex, m_MaterialSet.get());
    commandList->BindResourceSet(m_ObjectSetIndex, m_ObjectSet.get());
    commandList->BindMesh(meshBinding);
    commandList->DrawIndexed(3, 0, 0);
    commandList->EndRendering();
#endif
}

void HelloTriangle::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

void HelloTriangle::CreateTriangleResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    // Eigen vector constructors are not constexpr, so this array is only const.
    static const std::array<TriangleVertex, 3> kVertices = {{
        {{0.0f, -0.65f}, {1.0f, 0.25f, 0.25f, 1.0f}},
        {{0.65f, 0.55f}, {0.25f, 1.0f, 0.35f, 1.0f}},
        {{-0.65f, 0.55f}, {0.25f, 0.45f, 1.0f, 1.0f}},
    }};
    static constexpr std::array<uint16_t, 3> kIndices = {0, 1, 2};

    BufferDesc vertexBufferDesc;
    vertexBufferDesc.m_Size = sizeof(kVertices);
    vertexBufferDesc.m_UsageMask = BufferUsage::Vertex | BufferUsage::CopyDst;
    vertexBufferDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    vertexBufferDesc.m_DebugName = "HelloTriangle.VertexBuffer";
    m_VertexBuffer = device.CreateBuffer(vertexBufferDesc);

    BufferDesc vertexUploadBufferDesc;
    vertexUploadBufferDesc.m_Size = sizeof(kVertices);
    vertexUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    vertexUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    vertexUploadBufferDesc.m_DebugName = "HelloTriangle.VertexUploadBuffer";
    m_VertexUploadBuffer = device.CreateBuffer(vertexUploadBufferDesc);
    device.WriteBuffer(m_VertexUploadBuffer.get(), 0, kVertices.data(), sizeof(kVertices));

    BufferDesc indexBufferDesc;
    indexBufferDesc.m_Size = sizeof(kIndices);
    indexBufferDesc.m_UsageMask = BufferUsage::Index | BufferUsage::CopyDst;
    indexBufferDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    indexBufferDesc.m_DebugName = "HelloTriangle.IndexBuffer";
    m_IndexBuffer = device.CreateBuffer(indexBufferDesc);

    BufferDesc indexUploadBufferDesc;
    indexUploadBufferDesc.m_Size = sizeof(kIndices);
    indexUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    indexUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    indexUploadBufferDesc.m_DebugName = "HelloTriangle.IndexUploadBuffer";
    m_IndexUploadBuffer = device.CreateBuffer(indexUploadBufferDesc);
    device.WriteBuffer(m_IndexUploadBuffer.get(), 0, kIndices.data(), sizeof(kIndices));
    m_GeometryUploadPending = true;

    m_ShaderProgram = device.CreateShaderProgram(BuildHelloTriangleShaderProgramDesc());
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_FrameSetIndex = FindRequiredSetIndex(pipelineLayoutDesc, "gFrame");
    m_MaterialSetIndex = FindRequiredSetIndex(pipelineLayoutDesc, "gMaterial");
    m_ObjectSetIndex = FindRequiredSetIndex(pipelineLayoutDesc, "gObject");

    m_FrameSet = device.CreateResourceSet(m_PipelineLayout.get(), m_FrameSetIndex);
    m_MaterialSet = device.CreateResourceSet(m_PipelineLayout.get(), m_MaterialSetIndex);
    m_ObjectSet = device.CreateResourceSet(m_PipelineLayout.get(), m_ObjectSetIndex);

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());

    const Math::Mat4 viewProj = Math::Mat4::Identity();
    const Math::Vec4 baseColor = Math::Vec4(1.0f, 0.95f, 0.85f, 1.0f);

    parameterWriter.SetMatrix4x4(*m_FrameSet, "gFrame.viewProj", viewProj);
    parameterWriter.SetFloat4(*m_MaterialSet, "gMaterial.baseColor", baseColor);
    UpdateAnimatedParameters();

    VertexInputLayoutDesc vertexInputLayoutDesc;
    vertexInputLayoutDesc.m_Buffers = {{static_cast<uint32_t>(sizeof(TriangleVertex)), false}};
    vertexInputLayoutDesc.m_Attributes = {
        {0u, Format::RG32F, static_cast<uint32_t>(offsetof(TriangleVertex, m_Position)), 0u},
        {1u, Format::RGBA32F, static_cast<uint32_t>(offsetof(TriangleVertex, m_Color)), 0u},
    };
    m_VertexInputLayout = device.CreateVertexInputLayout(vertexInputLayoutDesc);

    GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.m_PipelineLayout = m_PipelineLayout.get();
    pipelineDesc.m_ShaderProgram = m_ShaderProgram.get();
    pipelineDesc.m_VertexInput = m_VertexInputLayout.get();
    pipelineDesc.m_RasterState.m_CullMode = CullMode::None;
    pipelineDesc.m_ColorFormats = {app.GetSwapchain().GetFormat()};
    m_GraphicsPipeline = device.CreateGraphicsPipeline(pipelineDesc);
#endif
}

void HelloTriangle::UploadTriangleGeometry(CommandList& commandList, ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GeometryUploadPending)
        return;

    RTRLAB_ASSERT_MSG(m_VertexUploadBuffer != nullptr && m_VertexBuffer != nullptr,
                      "HelloTriangle geometry upload requires both vertex staging and GPU buffers.");
    RTRLAB_ASSERT_MSG(m_IndexUploadBuffer != nullptr && m_IndexBuffer != nullptr,
                      "HelloTriangle geometry upload requires both index staging and GPU buffers.");

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

void HelloTriangle::ForgetTrackedBuffers()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}

void HelloTriangle::UpdateAnimatedParameters()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    RTRLAB_ASSERT_MSG(m_ShaderProgram != nullptr, "HelloTriangle animated parameters require a valid shader program.");
    RTRLAB_ASSERT_MSG(m_FrameSet != nullptr && m_MaterialSet != nullptr && m_ObjectSet != nullptr,
                      "HelloTriangle animated parameters require frame, material, and object resource sets.");

    const float timeSeconds = static_cast<float>(Time::GetTotalTime());
    const float pulse = 0.5f + 0.5f * std::sin(timeSeconds * 1.5f);
    const float verticalOffset = -0.04f + (pulse - 0.5f) * 0.10f;
    const float uniformScale = 0.88f + pulse * 0.10f;

    const Math::Vec4 tint = Math::Vec4(0.70f + 0.30f * pulse, 0.80f + 0.15f * pulse, 0.90f, 1.0f);
    const Math::Vec4 baseColor = Math::Vec4(1.0f, 0.70f + 0.25f * pulse, 0.70f + 0.20f * pulse, 1.0f);
    const Math::Mat4 model = Math::Translate(Math::Mat4::Identity(), Math::Vec3(0.0f, verticalOffset, 0.0f)) *
                             Math::Scale(Math::Mat4::Identity(), Math::Vec3(uniformScale, uniformScale, 1.0f));

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
    parameterWriter.SetFloat4(*m_FrameSet, "gFrame.tint", tint);
    parameterWriter.SetFloat(*m_FrameSet, "gFrame.time", timeSeconds);
    parameterWriter.SetFloat4(*m_MaterialSet, "gMaterial.baseColor", baseColor);
    parameterWriter.SetMatrix4x4(*m_ObjectSet, "gObject.model", model);
#endif
}
