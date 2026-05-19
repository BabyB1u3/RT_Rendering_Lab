#include "QuadDemo.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Util/Math.h"
#include "Demos/DemoRenderUtils.h"
#include "Render/RHI/RHIUpload.h"
#include "Render/Shader/ShaderParameterWriter.h"

QuadDemo::QuadDemo(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height) {}

void QuadDemo::OnAttach()
{
    CreateQuadResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "QuadDemo demo attached");
}

void QuadDemo::OnDetach()
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
    LOG_INFO_CAT(LogCategory::k_Demo, "QuadDemo demo detached");
}

void QuadDemo::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "QuadDemo requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "QuadDemo requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadQuadGeometry(*commandList, resourceStateTracker);

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

    const DemoRenderUtils::DemoViewport viewport =
        DemoRenderUtils::ComputeCenteredSquareViewport(m_ViewportWidth, m_ViewportHeight);
    commandList->SetViewport(viewport.m_X, viewport.m_Y, viewport.m_Width, viewport.m_Height, 0.0f, 1.0f);
    commandList->SetScissor(0, 0, m_ViewportWidth, m_ViewportHeight);
    commandList->BindGraphicsPipeline(m_GraphicsPipeline.get());
    commandList->BindResourceSet(m_FrameSetIndex, m_FrameSet.get());
    commandList->BindResourceSet(m_MaterialSetIndex, m_MaterialSet.get());
    commandList->BindResourceSet(m_ObjectSetIndex, m_ObjectSet.get());

    MeshBinding meshBinding;
    meshBinding.m_VertexBuffers = {m_VertexBuffer.get()};
    meshBinding.m_IndexBuffer = m_IndexBuffer.get();
    meshBinding.m_IndexType = IndexType::UInt16;
    commandList->BindMesh(meshBinding);
    commandList->DrawIndexed(6, 0, 0);
    commandList->EndRendering();
#endif
}

void QuadDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

void QuadDemo::CreateQuadResources()
{
#if defined(GLAB_BACKEND_VULKAN)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    using DemoRenderUtils::ColoredVertex;
    static const std::array<ColoredVertex, 4> kVertices = {{
        {{-0.72f, -0.72f, 0.0f, 1.0f}, {0.20f, 0.45f, 1.0f, 1.0f}},
        {{0.72f, -0.72f, 0.0f, 1.0f}, {0.20f, 0.90f, 0.50f, 1.0f}},
        {{0.72f, 0.72f, 0.0f, 1.0f}, {1.0f, 0.78f, 0.24f, 1.0f}},
        {{-0.72f, 0.72f, 0.0f, 1.0f}, {1.0f, 0.30f, 0.38f, 1.0f}},
    }};
    static constexpr std::array<uint16_t, 6> kIndices = {0, 1, 2, 0, 2, 3};

    RHIUpload::CreateStaticBufferPair(device,
                                      kVertices.data(),
                                      sizeof(kVertices),
                                      BufferUsage::Vertex,
                                      "QuadDemo.VertexBuffer",
                                      "QuadDemo.VertexUploadBuffer",
                                      m_VertexBuffer,
                                      m_VertexUploadBuffer);
    RHIUpload::CreateStaticBufferPair(device,
                                      kIndices.data(),
                                      sizeof(kIndices),
                                      BufferUsage::Index,
                                      "QuadDemo.IndexBuffer",
                                      "QuadDemo.IndexUploadBuffer",
                                      m_IndexBuffer,
                                      m_IndexUploadBuffer);
    m_GeometryUploadPending = true;

    m_ShaderProgram = device.CreateShaderProgram(DemoRenderUtils::CompileShaderProgramDesc(
        DemoRenderUtils::BuildGraphicsShaderCompileRequest(FileSystem::GetRootPath() / "Project" / "Shaders" /
                                                           "DemoColored.slang"),
        "QuadDemo"));
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_FrameSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gFrame", "QuadDemo");
    m_MaterialSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gMaterial", "QuadDemo");
    m_ObjectSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gObject", "QuadDemo");

    m_FrameSet = device.CreateResourceSet(m_PipelineLayout.get(), m_FrameSetIndex);
    m_MaterialSet = device.CreateResourceSet(m_PipelineLayout.get(), m_MaterialSetIndex);
    m_ObjectSet = device.CreateResourceSet(m_PipelineLayout.get(), m_ObjectSetIndex);

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
    parameterWriter.SetMatrix4x4(*m_FrameSet, "gFrame.viewProj", Math::Mat4::Identity());
    parameterWriter.SetFloat4(*m_FrameSet, "gFrame.tint", Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    parameterWriter.SetFloat(*m_FrameSet, "gFrame.time", 0.0f);
    parameterWriter.SetFloat4(*m_MaterialSet, "gMaterial.baseColor", Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    parameterWriter.SetMatrix4x4(*m_ObjectSet, "gObject.model", Math::Mat4::Identity());

    VertexInputLayoutDesc vertexInputLayoutDesc;
    vertexInputLayoutDesc.m_Buffers = {{static_cast<uint32_t>(sizeof(ColoredVertex)), false}};
    vertexInputLayoutDesc.m_Attributes = {
        {0u, Format::RGBA32F, static_cast<uint32_t>(offsetof(ColoredVertex, m_Position)), 0u},
        {1u, Format::RGBA32F, static_cast<uint32_t>(offsetof(ColoredVertex, m_Color)), 0u},
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

void QuadDemo::UploadQuadGeometry(CommandList& commandList, ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN)
    if (!m_GeometryUploadPending)
        return;

    RHIUpload::UploadStaticBufferPair(commandList,
                                      resourceStateTracker,
                                      m_VertexUploadBuffer.get(),
                                      m_VertexBuffer.get(),
                                      m_VertexBuffer->GetDesc().m_Size,
                                      BufferState::VertexIndex);
    RHIUpload::UploadStaticBufferPair(commandList,
                                      resourceStateTracker,
                                      m_IndexUploadBuffer.get(),
                                      m_IndexBuffer.get(),
                                      m_IndexBuffer->GetDesc().m_Size,
                                      BufferState::VertexIndex);
    m_GeometryUploadPending = false;
#endif
}

void QuadDemo::ForgetTrackedBuffers()
{
#if defined(GLAB_BACKEND_VULKAN)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}
