#include "TexturedRotatingCubeDemo.h"

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

TexturedRotatingCubeDemo::TexturedRotatingCubeDemo(uint32_t width, uint32_t height)
    : m_ViewportWidth(width), m_ViewportHeight(height)
{
}

void TexturedRotatingCubeDemo::OnAttach()
{
    CreateCubeResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "TexturedRotatingCube demo attached");
}

void TexturedRotatingCubeDemo::OnDetach()
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
    m_Sampler.reset();
    m_TextureView.reset();
    m_Texture.reset();
    m_TextureUploadBuffer.reset();
    m_IndexUploadBuffer.reset();
    m_VertexUploadBuffer.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
    m_GeometryUploadPending = false;
    m_TextureUploadPending = false;
    LOG_INFO_CAT(LogCategory::k_Demo, "TexturedRotatingCube demo detached");
}

void TexturedRotatingCubeDemo::OnUpdate(double dt)
{
    m_RotationSeconds += static_cast<float>(dt);
}

void TexturedRotatingCubeDemo::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer || !m_DepthView || !m_MaterialSet)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "TexturedRotatingCube requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "TexturedRotatingCube requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadPendingResources(*commandList, resourceStateTracker);
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

void TexturedRotatingCubeDemo::OnResize(uint32_t width, uint32_t height)
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

void TexturedRotatingCubeDemo::CreateCubeResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    using DemoRenderUtils::TexturedVertex;
    static const std::array<TexturedVertex, 24> kVertices = {{
        {{-0.6f, -0.6f, 0.6f, 1.0f}, {0.0f, 1.0f}},  {{0.6f, -0.6f, 0.6f, 1.0f}, {1.0f, 1.0f}},
        {{0.6f, 0.6f, 0.6f, 1.0f}, {1.0f, 0.0f}},    {{-0.6f, 0.6f, 0.6f, 1.0f}, {0.0f, 0.0f}},

        {{0.6f, -0.6f, -0.6f, 1.0f}, {0.0f, 1.0f}},  {{-0.6f, -0.6f, -0.6f, 1.0f}, {1.0f, 1.0f}},
        {{-0.6f, 0.6f, -0.6f, 1.0f}, {1.0f, 0.0f}},  {{0.6f, 0.6f, -0.6f, 1.0f}, {0.0f, 0.0f}},

        {{-0.6f, -0.6f, -0.6f, 1.0f}, {0.0f, 1.0f}}, {{-0.6f, -0.6f, 0.6f, 1.0f}, {1.0f, 1.0f}},
        {{-0.6f, 0.6f, 0.6f, 1.0f}, {1.0f, 0.0f}},   {{-0.6f, 0.6f, -0.6f, 1.0f}, {0.0f, 0.0f}},

        {{0.6f, -0.6f, 0.6f, 1.0f}, {0.0f, 1.0f}},   {{0.6f, -0.6f, -0.6f, 1.0f}, {1.0f, 1.0f}},
        {{0.6f, 0.6f, -0.6f, 1.0f}, {1.0f, 0.0f}},   {{0.6f, 0.6f, 0.6f, 1.0f}, {0.0f, 0.0f}},

        {{-0.6f, 0.6f, 0.6f, 1.0f}, {0.0f, 1.0f}},   {{0.6f, 0.6f, 0.6f, 1.0f}, {1.0f, 1.0f}},
        {{0.6f, 0.6f, -0.6f, 1.0f}, {1.0f, 0.0f}},   {{-0.6f, 0.6f, -0.6f, 1.0f}, {0.0f, 0.0f}},

        {{-0.6f, -0.6f, -0.6f, 1.0f}, {0.0f, 1.0f}}, {{0.6f, -0.6f, -0.6f, 1.0f}, {1.0f, 1.0f}},
        {{0.6f, -0.6f, 0.6f, 1.0f}, {1.0f, 0.0f}},   {{-0.6f, -0.6f, 0.6f, 1.0f}, {0.0f, 0.0f}},
    }};
    static constexpr std::array<uint16_t, 36> kIndices = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    DemoRenderUtils::CreateStaticBufferPair(device,
                                            kVertices.data(),
                                            sizeof(kVertices),
                                            BufferUsage::Vertex,
                                            "TexturedRotatingCube.VertexBuffer",
                                            "TexturedRotatingCube.VertexUploadBuffer",
                                            m_VertexBuffer,
                                            m_VertexUploadBuffer);
    DemoRenderUtils::CreateStaticBufferPair(device,
                                            kIndices.data(),
                                            sizeof(kIndices),
                                            BufferUsage::Index,
                                            "TexturedRotatingCube.IndexBuffer",
                                            "TexturedRotatingCube.IndexUploadBuffer",
                                            m_IndexBuffer,
                                            m_IndexUploadBuffer);
    m_GeometryUploadPending = true;
    CreateDepthResources();

    const std::filesystem::path texturePath = FileSystem::GetRootPath() / "Project" / "Textures" / "Grassy_Square.jpg";
    const DemoRenderUtils::LoadedImage loadedImage =
        DemoRenderUtils::LoadTextureFileRGBA8(texturePath, "TexturedRotatingCube");

    TextureDesc textureDesc;
    textureDesc.m_Type = TextureType::Tex2D;
    textureDesc.m_Format = Format::RGBA8_UNORM;
    textureDesc.m_Extent = {loadedImage.m_Width, loadedImage.m_Height, 1};
    textureDesc.m_MipLevels = 1;
    textureDesc.m_ArrayLayers = 1;
    textureDesc.m_UsageMask = TextureUsage::Sampled | TextureUsage::CopyDst;
    textureDesc.m_DebugName = "TexturedRotatingCube.ColorTexture";
    m_Texture = device.CreateTexture(textureDesc);

    TextureViewDesc textureViewDesc;
    textureViewDesc.m_Type = TextureType::Tex2D;
    textureViewDesc.m_Format = textureDesc.m_Format;
    textureViewDesc.m_Aspect = TextureAspect::Color;
    m_TextureView = device.CreateTextureView(m_Texture.get(), textureViewDesc);

    SamplerDesc samplerDesc;
    samplerDesc.m_MinFilter = FilterMode::Linear;
    samplerDesc.m_MagFilter = FilterMode::Linear;
    samplerDesc.m_MipFilter = MipFilterMode::Linear;
    samplerDesc.m_AddressU = AddressMode::Repeat;
    samplerDesc.m_AddressV = AddressMode::Repeat;
    samplerDesc.m_AddressW = AddressMode::Repeat;
    m_Sampler = device.CreateSampler(samplerDesc);

    m_TextureUploadBuffer = RHIUpload::CreateUploadBuffer(device,
                                                          loadedImage.m_Pixels.data(),
                                                          static_cast<uint64_t>(loadedImage.m_Pixels.size()),
                                                          "TexturedRotatingCube.TextureUploadBuffer");
    m_TextureUploadPending = true;

    m_ShaderProgram = device.CreateShaderProgram(DemoRenderUtils::CompileShaderProgramDesc(
        DemoRenderUtils::BuildGraphicsShaderCompileRequest(FileSystem::GetRootPath() / "Project" / "Shaders" /
                                                           "DemoTextured.slang"),
        "TexturedRotatingCube"));
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_FrameSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gFrame", "TexturedRotatingCube");
    m_MaterialSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gMaterial", "TexturedRotatingCube");
    m_ObjectSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gObject", "TexturedRotatingCube");

    m_FrameSet = device.CreateResourceSet(m_PipelineLayout.get(), m_FrameSetIndex);
    m_MaterialSet = device.CreateResourceSet(m_PipelineLayout.get(), m_MaterialSetIndex);
    m_ObjectSet = device.CreateResourceSet(m_PipelineLayout.get(), m_ObjectSetIndex);
    UpdateShaderParameters();

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
    parameterWriter.SetTextureView(*m_MaterialSet, "gAlbedoTexture", m_TextureView.get());
    parameterWriter.SetSampler(*m_MaterialSet, "gAlbedoSampler", m_Sampler.get());

    VertexInputLayoutDesc vertexInputLayoutDesc;
    vertexInputLayoutDesc.m_Buffers = {{static_cast<uint32_t>(sizeof(TexturedVertex)), false}};
    vertexInputLayoutDesc.m_Attributes = {
        {0u, Format::RGBA32F, static_cast<uint32_t>(offsetof(TexturedVertex, m_Position)), 0u},
        {1u, Format::RG32F, static_cast<uint32_t>(offsetof(TexturedVertex, m_UV)), 0u},
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

void TexturedRotatingCubeDemo::CreateDepthResources()
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
    depthDesc.m_DebugName = "TexturedRotatingCube.Depth";
    m_DepthTexture = device.CreateTexture(depthDesc);

    TextureViewDesc depthViewDesc;
    depthViewDesc.m_Type = TextureType::Tex2D;
    depthViewDesc.m_Format = depthDesc.m_Format;
    depthViewDesc.m_Aspect = TextureAspect::Depth;
    m_DepthView = device.CreateTextureView(m_DepthTexture.get(), depthViewDesc);
#endif
}

void TexturedRotatingCubeDemo::UploadPendingResources(CommandList& commandList,
                                                      ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (m_GeometryUploadPending)
    {
        DemoRenderUtils::UploadStaticBufferPair(commandList,
                                                resourceStateTracker,
                                                m_VertexUploadBuffer.get(),
                                                m_VertexBuffer.get(),
                                                m_VertexBuffer->GetDesc().m_Size,
                                                BufferState::VertexIndex);
        DemoRenderUtils::UploadStaticBufferPair(commandList,
                                                resourceStateTracker,
                                                m_IndexUploadBuffer.get(),
                                                m_IndexBuffer.get(),
                                                m_IndexBuffer->GetDesc().m_Size,
                                                BufferState::VertexIndex);
        m_GeometryUploadPending = false;
    }

    if (m_TextureUploadPending)
    {
        RHIUpload::UploadFullTexture(commandList,
                                     resourceStateTracker,
                                     m_TextureUploadBuffer.get(),
                                     m_Texture.get(),
                                     4u,
                                     TextureState::ShaderRead);
        m_TextureUploadPending = false;
    }
#endif
}

void TexturedRotatingCubeDemo::UpdateShaderParameters()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    RTRLAB_ASSERT_MSG(m_ShaderProgram != nullptr && m_FrameSet != nullptr && m_MaterialSet != nullptr &&
                          m_ObjectSet != nullptr,
                      "TexturedRotatingCube parameter updates require initialized shader resources.");

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

void TexturedRotatingCubeDemo::ForgetTrackedResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_DepthTexture.get());
    resourceStateTracker.Forget(m_Texture.get());
    resourceStateTracker.Forget(m_TextureUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}
