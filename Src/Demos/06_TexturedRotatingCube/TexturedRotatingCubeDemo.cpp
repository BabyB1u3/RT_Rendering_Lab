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
    m_ObjectSet.reset();
    m_MaterialSet.reset();
    m_Renderer.Reset();
    m_RenderObject = {};
    m_Material = {};
    m_Mesh = {};
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
    if (!m_Renderer.IsInitialized() || !m_VertexBuffer || !m_IndexBuffer || !m_DepthView ||
        m_RenderObject.m_Mesh == nullptr || m_RenderObject.m_Material == nullptr)
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
    m_Renderer.DrawObject(*commandList, m_RenderObject);
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
    if (m_Renderer.GetFrameSet() != nullptr)
    {
        UpdateShaderParameters();
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
    const std::filesystem::path rootPath = FileSystem::GetRootPath();

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

    RHIUpload::CreateStaticBufferPair(device,
                                      kVertices.data(),
                                      sizeof(kVertices),
                                      BufferUsage::Vertex,
                                      "TexturedRotatingCube.VertexBuffer",
                                      "TexturedRotatingCube.VertexUploadBuffer",
                                      m_VertexBuffer,
                                      m_VertexUploadBuffer);
    RHIUpload::CreateStaticBufferPair(device,
                                      kIndices.data(),
                                      sizeof(kIndices),
                                      BufferUsage::Index,
                                      "TexturedRotatingCube.IndexBuffer",
                                      "TexturedRotatingCube.IndexUploadBuffer",
                                      m_IndexBuffer,
                                      m_IndexUploadBuffer);
    m_GeometryUploadPending = true;

    const std::filesystem::path texturePath = rootPath / "Project" / "Textures" / "Grassy_Square.jpg";
    const DemoRenderUtils::LoadedImage textureImage =
        DemoRenderUtils::LoadTextureFileRGBA8(texturePath, "TexturedRotatingCube");

    DemoRenderUtils::CreateRGBA8Texture2DWithView(device,
                                                  textureImage.m_Width,
                                                  textureImage.m_Height,
                                                  "TexturedRotatingCube.ColorTexture",
                                                  "TexturedRotatingCube.ColorTextureView",
                                                  m_Texture,
                                                  m_TextureView);

    m_Sampler = DemoRenderUtils::CreateLinearRepeatSampler(device, "TexturedRotatingCube.LinearRepeatSampler");

    m_TextureUploadBuffer = RHIUpload::CreateUploadBuffer(device,
                                                          textureImage.m_Pixels.data(),
                                                          static_cast<uint64_t>(textureImage.m_Pixels.size()),
                                                          "TexturedRotatingCube.TextureUploadBuffer");
    m_TextureUploadPending = true;

    CreateDepthResources();

    const std::filesystem::path shaderPath = rootPath / "Project" / "Shaders" / "DemoTextured.slang";
    Renderer::ForwardRendererDesc rendererDesc;
    rendererDesc.m_ShaderPath = shaderPath;
    rendererDesc.m_ColorFormat = app.GetSwapchain().GetFormat();
    rendererDesc.m_DepthFormat = Format::D32_SFLOAT;
    rendererDesc.m_VertexStride = static_cast<uint32_t>(sizeof(TexturedVertex));
    rendererDesc.m_PositionOffset = static_cast<uint32_t>(offsetof(TexturedVertex, m_Position));
    rendererDesc.m_UVOffset = static_cast<uint32_t>(offsetof(TexturedVertex, m_UV));
    rendererDesc.m_DebugName = "TexturedRotatingCube";
    m_Renderer.Initialize(device, rendererDesc);

    m_MaterialSet = m_Renderer.CreateMaterialSet(device);
    m_ObjectSet = m_Renderer.CreateObjectSet(device);

    m_Mesh.m_VertexBuffer = m_VertexBuffer.get();
    m_Mesh.m_IndexBuffer = m_IndexBuffer.get();
    m_Mesh.m_IndexType = IndexType::UInt16;
    m_Mesh.m_IndexCount = 36;

    m_Material.m_BaseColor = Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    m_Material.m_AlbedoTextureView = m_TextureView.get();
    m_Material.m_AlbedoSampler = m_Sampler.get();
    m_Material.m_ResourceSet = m_MaterialSet.get();

    m_RenderObject.m_Mesh = &m_Mesh;
    m_RenderObject.m_Material = &m_Material;
    m_RenderObject.m_ObjectSet = m_ObjectSet.get();
    UpdateShaderParameters();
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

void TexturedRotatingCubeDemo::UpdateShaderParameters()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    RTRLAB_ASSERT_MSG(m_Renderer.IsInitialized() && m_RenderObject.m_Mesh != nullptr &&
                          m_RenderObject.m_Material != nullptr && m_RenderObject.m_ObjectSet != nullptr,
                      "TexturedRotatingCube parameter updates require initialized shader resources.");

    const Math::Mat4 model =
        Math::Rotate(Math::Mat4::Identity(), m_RotationSeconds * 1.15f, Math::Vec3(0.0f, 1.0f, 0.0f)) *
        Math::Rotate(Math::Mat4::Identity(), m_RotationSeconds * 0.70f, Math::Vec3(1.0f, 0.0f, 0.0f));

    Renderer::FrameGlobals frameGlobals;
    frameGlobals.m_ViewProjection = DemoRenderUtils::BuildOrbitViewProjection(m_ViewportWidth, m_ViewportHeight);
    frameGlobals.m_Tint = Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    frameGlobals.m_Time = m_RotationSeconds;

    m_RenderObject.m_Model = model;
    m_Renderer.UpdateFrameGlobals(frameGlobals);
    m_Renderer.UpdateMaterial(m_Material);
    m_Renderer.UpdateRenderObject(m_RenderObject);
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
