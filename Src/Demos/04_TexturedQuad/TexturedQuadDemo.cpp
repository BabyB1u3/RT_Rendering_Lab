#include "TexturedQuadDemo.h"

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

TexturedQuadDemo::TexturedQuadDemo(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height)
{
}

void TexturedQuadDemo::OnAttach()
{
    CreateTexturedQuadResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "TexturedQuadDemo demo attached");
}

void TexturedQuadDemo::OnDetach()
{
    ForgetTrackedResources();
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_ObjectSet.reset();
    m_MaterialSet.reset();
    m_FrameSet.reset();
    m_PipelineLayout.reset();
    m_ShaderProgram.reset();
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
    LOG_INFO_CAT(LogCategory::k_Demo, "TexturedQuadDemo demo detached");
}

void TexturedQuadDemo::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer || !m_MaterialSet)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "TexturedQuadDemo requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "TexturedQuadDemo requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadPendingResources(*commandList, resourceStateTracker);

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

void TexturedQuadDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

void TexturedQuadDemo::CreateTexturedQuadResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    using DemoRenderUtils::TexturedVertex;
    static const std::array<TexturedVertex, 4> kVertices = {{
        {{-0.78f, -0.78f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{0.78f, -0.78f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{0.78f, 0.78f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.78f, 0.78f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    }};
    static constexpr std::array<uint16_t, 6> kIndices = {0, 1, 2, 0, 2, 3};

    RHIUpload::CreateStaticBufferPair(device,
                                      kVertices.data(),
                                      sizeof(kVertices),
                                      BufferUsage::Vertex,
                                      "TexturedQuadDemo.VertexBuffer",
                                      "TexturedQuadDemo.VertexUploadBuffer",
                                      m_VertexBuffer,
                                      m_VertexUploadBuffer);
    RHIUpload::CreateStaticBufferPair(device,
                                      kIndices.data(),
                                      sizeof(kIndices),
                                      BufferUsage::Index,
                                      "TexturedQuadDemo.IndexBuffer",
                                      "TexturedQuadDemo.IndexUploadBuffer",
                                      m_IndexBuffer,
                                      m_IndexUploadBuffer);
    m_GeometryUploadPending = true;

    const std::filesystem::path texturePath = FileSystem::GetRootPath() / "Project" / "Textures" / "Grassy_Square.jpg";
    const DemoRenderUtils::LoadedImage loadedImage =
        DemoRenderUtils::LoadTextureFileRGBA8(texturePath, "TexturedQuadDemo");

    DemoRenderUtils::CreateRGBA8Texture2DWithView(device,
                                                  loadedImage.m_Width,
                                                  loadedImage.m_Height,
                                                  "TexturedQuadDemo.ColorTexture",
                                                  "TexturedQuadDemo.ColorTextureView",
                                                  m_Texture,
                                                  m_TextureView);

    m_Sampler = DemoRenderUtils::CreateLinearRepeatSampler(device, "TexturedQuadDemo.LinearRepeatSampler");

    m_TextureUploadBuffer = RHIUpload::CreateUploadBuffer(device,
                                                          loadedImage.m_Pixels.data(),
                                                          static_cast<uint64_t>(loadedImage.m_Pixels.size()),
                                                          "TexturedQuadDemo.TextureUploadBuffer");
    m_TextureUploadPending = true;

    m_ShaderProgram = device.CreateShaderProgram(DemoRenderUtils::CompileShaderProgramDesc(
        DemoRenderUtils::BuildGraphicsShaderCompileRequest(FileSystem::GetRootPath() / "Project" / "Shaders" /
                                                           "DemoTextured.slang"),
        "TexturedQuadDemo"));
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_FrameSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gFrame", "TexturedQuadDemo");
    m_MaterialSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gMaterial", "TexturedQuadDemo");
    m_ObjectSetIndex = DemoRenderUtils::FindRequiredSetIndex(pipelineLayoutDesc, "gObject", "TexturedQuadDemo");

    m_FrameSet = device.CreateResourceSet(m_PipelineLayout.get(), m_FrameSetIndex);
    m_MaterialSet = device.CreateResourceSet(m_PipelineLayout.get(), m_MaterialSetIndex);
    m_ObjectSet = device.CreateResourceSet(m_PipelineLayout.get(), m_ObjectSetIndex);

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
    parameterWriter.SetMatrix4x4(*m_FrameSet, "gFrame.viewProj", Math::Mat4::Identity());
    parameterWriter.SetFloat4(*m_FrameSet, "gFrame.tint", Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    parameterWriter.SetFloat(*m_FrameSet, "gFrame.time", 0.0f);
    parameterWriter.SetFloat4(*m_MaterialSet, "gMaterial.baseColor", Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    parameterWriter.SetTextureView(*m_MaterialSet, "gAlbedoTexture", m_TextureView.get());
    parameterWriter.SetSampler(*m_MaterialSet, "gAlbedoSampler", m_Sampler.get());
    parameterWriter.SetMatrix4x4(*m_ObjectSet, "gObject.model", Math::Mat4::Identity());

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
    pipelineDesc.m_ColorFormats = {app.GetSwapchain().GetFormat()};
    m_GraphicsPipeline = device.CreateGraphicsPipeline(pipelineDesc);
#endif
}

void TexturedQuadDemo::UploadPendingResources(CommandList& commandList, ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (m_GeometryUploadPending)
    {
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

void TexturedQuadDemo::ForgetTrackedResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_Texture.get());
    resourceStateTracker.Forget(m_TextureUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}
