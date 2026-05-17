#include "RotatingCubeDemo.h"

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

RotatingCubeDemo::RotatingCubeDemo(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height)
{
}

void RotatingCubeDemo::OnAttach()
{
    CreateCubeResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "RotatingCube demo attached");
}

void RotatingCubeDemo::OnDetach()
{
    ForgetTrackedResources();
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_ObjectSet.reset();
    m_MaterialSet.reset();
    m_FrameSet.reset();
    m_PipelineLayout.reset();
    m_ShaderProgram.reset();
    m_DepthView.reset();
    m_DepthTexture.reset();
    m_IndexUploadBuffer.reset();
    m_VertexUploadBuffer.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
    m_GeometryUploadPending = false;
    LOG_INFO_CAT(LogCategory::k_Demo, "RotatingCube demo detached");
}

void RotatingCubeDemo::OnUpdate(double dt)
{
    m_RotationSeconds += static_cast<float>(dt);
}

void RotatingCubeDemo::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer || !m_DepthView)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "RotatingCube requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "RotatingCube requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadCubeGeometry(*commandList, resourceStateTracker);
    UpdateShaderParameters();

    resourceStateTracker.Transition(swapchainImage, TextureState::RenderTarget);
    resourceStateTracker.Transition(m_DepthTexture.get(), TextureState::DepthStencil);
    resourceStateTracker.FlushBarriers(commandList);

    ColorAttachmentInfo colorAttachment;
    colorAttachment.m_View = swapchainImageView;
    colorAttachment.m_LoadOp = LoadOp::Clear;
    colorAttachment.m_StoreOp = StoreOp::Store;
    colorAttachment.m_ClearValue = {0.08f, 0.10f, 0.12f, 1.0f};

    DepthAttachmentInfo depthAttachment;
    depthAttachment.m_View = m_DepthView.get();
    depthAttachment.m_LoadOp = LoadOp::Clear;
    depthAttachment.m_StoreOp = StoreOp::Store;
    depthAttachment.m_ClearValue = {1.0f, 0};

    RenderingInfo renderingInfo;
    renderingInfo.m_ColorAttachments = {colorAttachment};
    renderingInfo.m_DepthAttachment = depthAttachment;
    renderingInfo.m_RenderArea = {0, 0, m_ViewportWidth, m_ViewportHeight};
    commandList->BeginRendering(renderingInfo);

    commandList->SetViewport(
        0.0f, 0.0f, static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight), 0.0f, 1.0f);
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
    commandList->DrawIndexed(36, 0, 0);
    commandList->EndRendering();
#endif
}

void RotatingCubeDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (m_FrameSet != nullptr)
    {
        ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
        parameterWriter.SetMatrix4x4(*m_FrameSet,
                                     "gFrame.viewProj",
                                     DemoRenderUtils::BuildOrbitViewProjection(m_ViewportWidth, m_ViewportHeight));
    }

    if (m_DepthTexture != nullptr)
        CreateDepthResources();
#endif
}

void RotatingCubeDemo::CreateCubeResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    using DemoRenderUtils::ColoredVertex;
    static const std::array<ColoredVertex, 24> kVertices = {{
        {{-0.6f, -0.6f, 0.6f, 1.0f}, {1.0f, 0.25f, 0.25f, 1.0f}},
        {{0.6f, -0.6f, 0.6f, 1.0f}, {1.0f, 0.25f, 0.25f, 1.0f}},
        {{0.6f, 0.6f, 0.6f, 1.0f}, {1.0f, 0.25f, 0.25f, 1.0f}},
        {{-0.6f, 0.6f, 0.6f, 1.0f}, {1.0f, 0.25f, 0.25f, 1.0f}},

        {{0.6f, -0.6f, -0.6f, 1.0f}, {0.25f, 0.95f, 0.45f, 1.0f}},
        {{-0.6f, -0.6f, -0.6f, 1.0f}, {0.25f, 0.95f, 0.45f, 1.0f}},
        {{-0.6f, 0.6f, -0.6f, 1.0f}, {0.25f, 0.95f, 0.45f, 1.0f}},
        {{0.6f, 0.6f, -0.6f, 1.0f}, {0.25f, 0.95f, 0.45f, 1.0f}},

        {{-0.6f, -0.6f, -0.6f, 1.0f}, {0.25f, 0.45f, 1.0f, 1.0f}},
        {{-0.6f, -0.6f, 0.6f, 1.0f}, {0.25f, 0.45f, 1.0f, 1.0f}},
        {{-0.6f, 0.6f, 0.6f, 1.0f}, {0.25f, 0.45f, 1.0f, 1.0f}},
        {{-0.6f, 0.6f, -0.6f, 1.0f}, {0.25f, 0.45f, 1.0f, 1.0f}},

        {{0.6f, -0.6f, 0.6f, 1.0f}, {1.0f, 0.82f, 0.25f, 1.0f}},
        {{0.6f, -0.6f, -0.6f, 1.0f}, {1.0f, 0.82f, 0.25f, 1.0f}},
        {{0.6f, 0.6f, -0.6f, 1.0f}, {1.0f, 0.82f, 0.25f, 1.0f}},
        {{0.6f, 0.6f, 0.6f, 1.0f}, {1.0f, 0.82f, 0.25f, 1.0f}},

        {{-0.6f, 0.6f, 0.6f, 1.0f}, {0.90f, 0.35f, 1.0f, 1.0f}},
        {{0.6f, 0.6f, 0.6f, 1.0f}, {0.90f, 0.35f, 1.0f, 1.0f}},
        {{0.6f, 0.6f, -0.6f, 1.0f}, {0.90f, 0.35f, 1.0f, 1.0f}},
        {{-0.6f, 0.6f, -0.6f, 1.0f}, {0.90f, 0.35f, 1.0f, 1.0f}},

        {{-0.6f, -0.6f, -0.6f, 1.0f}, {0.20f, 0.95f, 0.95f, 1.0f}},
        {{0.6f, -0.6f, -0.6f, 1.0f}, {0.20f, 0.95f, 0.95f, 1.0f}},
        {{0.6f, -0.6f, 0.6f, 1.0f}, {0.20f, 0.95f, 0.95f, 1.0f}},
        {{-0.6f, -0.6f, 0.6f, 1.0f}, {0.20f, 0.95f, 0.95f, 1.0f}},
    }};
    static constexpr std::array<uint16_t, 36> kIndices = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    RHIUpload::CreateStaticBufferPair(device,
                                      kVertices.data(),
                                      sizeof(kVertices),
                                      BufferUsage::Vertex,
                                      "RotatingCube.VertexBuffer",
                                      "RotatingCube.VertexUploadBuffer",
                                      m_VertexBuffer,
                                      m_VertexUploadBuffer);
    RHIUpload::CreateStaticBufferPair(device,
                                      kIndices.data(),
                                      sizeof(kIndices),
                                      BufferUsage::Index,
                                      "RotatingCube.IndexBuffer",
                                      "RotatingCube.IndexUploadBuffer",
                                      m_IndexBuffer,
                                      m_IndexUploadBuffer);
    m_GeometryUploadPending = true;
    CreateDepthResources();

    m_ShaderProgram = device.CreateShaderProgram(DemoRenderUtils::CompileShaderProgramDesc(
        DemoRenderUtils::BuildGraphicsShaderCompileRequest(FileSystem::GetRootPath() / "Project" / "Shaders" /
                                                           "DemoColored.slang"),
        "RotatingCube"));
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_FrameSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gFrame", "RotatingCube");
    m_MaterialSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gMaterial", "RotatingCube");
    m_ObjectSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gObject", "RotatingCube");

    m_FrameSet = device.CreateResourceSet(m_PipelineLayout.get(), m_FrameSetIndex);
    m_MaterialSet = device.CreateResourceSet(m_PipelineLayout.get(), m_MaterialSetIndex);
    m_ObjectSet = device.CreateResourceSet(m_PipelineLayout.get(), m_ObjectSetIndex);
    UpdateShaderParameters();

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
    pipelineDesc.m_DepthStencilState.m_DepthTestEnable = true;
    pipelineDesc.m_DepthStencilState.m_DepthWriteEnable = true;
    pipelineDesc.m_DepthStencilState.m_DepthCompareOp = CompareOp::Less;
    pipelineDesc.m_ColorFormats = {app.GetSwapchain().GetFormat()};
    pipelineDesc.m_DepthFormat = Format::D32_SFLOAT;
    m_GraphicsPipeline = device.CreateGraphicsPipeline(pipelineDesc);
#endif
}

void RotatingCubeDemo::CreateDepthResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (m_DepthTexture != nullptr)
        Application::Get().GetResourceStateTracker().Forget(m_DepthTexture.get());

    m_DepthView.reset();
    m_DepthTexture.reset();

    Device& device = Application::Get().GetDevice();

    TextureDesc depthDesc;
    depthDesc.m_Type = TextureType::Tex2D;
    depthDesc.m_Format = Format::D32_SFLOAT;
    depthDesc.m_Extent = {m_ViewportWidth, m_ViewportHeight, 1};
    depthDesc.m_MipLevels = 1;
    depthDesc.m_ArrayLayers = 1;
    depthDesc.m_UsageMask = TextureUsage::DepthStencil;
    depthDesc.m_DebugName = "RotatingCube.Depth";
    m_DepthTexture = device.CreateTexture(depthDesc);

    TextureViewDesc depthViewDesc;
    depthViewDesc.m_Type = TextureType::Tex2D;
    depthViewDesc.m_Format = depthDesc.m_Format;
    depthViewDesc.m_Aspect = TextureAspect::Depth;
    m_DepthView = device.CreateTextureView(m_DepthTexture.get(), depthViewDesc);
#endif
}

void RotatingCubeDemo::UploadCubeGeometry(CommandList& commandList, ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
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

void RotatingCubeDemo::UpdateShaderParameters()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    RTRLAB_ASSERT_MSG(m_ShaderProgram != nullptr && m_FrameSet != nullptr && m_MaterialSet != nullptr &&
                          m_ObjectSet != nullptr,
                      "RotatingCube parameter updates require initialized shader resources.");

    const Math::Mat4 model =
        Math::Rotate(Math::Mat4::Identity(), m_RotationSeconds * 1.15f, Math::Vec3(0.0f, 1.0f, 0.0f)) *
        Math::Rotate(Math::Mat4::Identity(), m_RotationSeconds * 0.70f, Math::Vec3(1.0f, 0.0f, 0.0f));

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
    parameterWriter.SetMatrix4x4(
        *m_FrameSet, "gFrame.viewProj", DemoRenderUtils::BuildOrbitViewProjection(m_ViewportWidth, m_ViewportHeight));
    parameterWriter.SetFloat4(*m_FrameSet, "gFrame.tint", Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    parameterWriter.SetFloat(*m_FrameSet, "gFrame.time", m_RotationSeconds);
    parameterWriter.SetFloat4(*m_MaterialSet, "gMaterial.baseColor", Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    parameterWriter.SetMatrix4x4(*m_ObjectSet, "gObject.model", model);
#endif
}

void RotatingCubeDemo::ForgetTrackedResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_DepthTexture.get());
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}
