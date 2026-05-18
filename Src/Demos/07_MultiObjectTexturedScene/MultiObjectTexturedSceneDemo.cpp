#include "MultiObjectTexturedSceneDemo.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Util/Math.h"
#include "Demos/DemoRenderUtils.h"
#include "Render/RHI/RHIUpload.h"

MultiObjectTexturedSceneDemo::MultiObjectTexturedSceneDemo(uint32_t width, uint32_t height)
    : m_ViewportWidth(width), m_ViewportHeight(height)
{
}

void MultiObjectTexturedSceneDemo::OnAttach()
{
    CreateSceneResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "MultiObjectTexturedScene demo attached");
}

void MultiObjectTexturedSceneDemo::OnDetach()
{
    ForgetTrackedResources();
    for (Scope<ResourceSet>& objectSet : m_ObjectSets)
        objectSet.reset();
    for (Scope<ResourceSet>& materialSet : m_MaterialSets)
        materialSet.reset();
    m_Renderer.Reset();
    m_RenderObjects = {};
    m_Materials = {};
    m_Mesh = {};
    m_DepthView.reset();
    m_DepthTexture.reset();
    m_Sampler.reset();
    for (Scope<TextureView>& textureView : m_TextureViews)
        textureView.reset();
    for (Scope<Texture>& texture : m_Textures)
        texture.reset();
    for (Scope<Buffer>& textureUploadBuffer : m_TextureUploadBuffers)
        textureUploadBuffer.reset();
    m_IndexUploadBuffer.reset();
    m_VertexUploadBuffer.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
    m_GeometryUploadPending = false;
    m_TextureUploadPending = {};
    LOG_INFO_CAT(LogCategory::k_Demo, "MultiObjectTexturedScene demo detached");
}

void MultiObjectTexturedSceneDemo::OnUpdate(double dt)
{
    m_ElapsedSeconds += static_cast<float>(dt);
}

void MultiObjectTexturedSceneDemo::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_Renderer.IsInitialized() || !m_VertexBuffer || !m_IndexBuffer || !m_DepthView ||
        m_RenderObjects.front().m_Mesh == nullptr || m_RenderObjects.front().m_Material == nullptr)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(commandList != nullptr,
                      "MultiObjectTexturedScene requires an active command list during OnRender.");
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "MultiObjectTexturedScene requires the application to expose the active swapchain backbuffer.");

    ResourceStateTracker& resourceStateTracker = app.GetResourceStateTracker();
    UploadPendingResources(*commandList, resourceStateTracker);
    UpdateSceneParameters();

    resourceStateTracker.Transition(swapchainImage, TextureState::RenderTarget);
    resourceStateTracker.Transition(m_DepthTexture.get(), TextureState::DepthStencil);
    resourceStateTracker.FlushBarriers(commandList);

    ColorAttachmentInfo colorAttachment;
    colorAttachment.m_View = swapchainImageView;
    colorAttachment.m_LoadOp = LoadOp::Clear;
    colorAttachment.m_StoreOp = StoreOp::Store;
    colorAttachment.m_ClearValue = {0.06f, 0.07f, 0.09f, 1.0f};

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
    for (const Renderer::RenderObject& object : m_RenderObjects)
        m_Renderer.DrawObject(*commandList, object);
    commandList->EndRendering();
#endif
}

void MultiObjectTexturedSceneDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (m_Renderer.GetFrameSet() != nullptr)
        UpdateSceneParameters();

    if (m_DepthTexture != nullptr)
        CreateDepthResources();
#endif
}

void MultiObjectTexturedSceneDemo::CreateSceneResources()
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
                                      "MultiObjectTexturedScene.VertexBuffer",
                                      "MultiObjectTexturedScene.VertexUploadBuffer",
                                      m_VertexBuffer,
                                      m_VertexUploadBuffer);
    RHIUpload::CreateStaticBufferPair(device,
                                      kIndices.data(),
                                      sizeof(kIndices),
                                      BufferUsage::Index,
                                      "MultiObjectTexturedScene.IndexBuffer",
                                      "MultiObjectTexturedScene.IndexUploadBuffer",
                                      m_IndexBuffer,
                                      m_IndexUploadBuffer);
    m_GeometryUploadPending = true;

    const std::array<std::filesystem::path, kMaterialCount> texturePaths = {
        rootPath / "Project" / "Textures" / "Grassy_Square.jpg",
        rootPath / "Project" / "Textures" / "Metal_Square.jpg",
    };
    static const std::array<const char*, kMaterialCount> kTextureDebugNames = {
        "MultiObjectTexturedScene.GrassTexture",
        "MultiObjectTexturedScene.MetalTexture",
    };
    static const std::array<const char*, kMaterialCount> kTextureViewDebugNames = {
        "MultiObjectTexturedScene.GrassTextureView",
        "MultiObjectTexturedScene.MetalTextureView",
    };
    static const std::array<const char*, kMaterialCount> kTextureUploadDebugNames = {
        "MultiObjectTexturedScene.GrassTextureUploadBuffer",
        "MultiObjectTexturedScene.MetalTextureUploadBuffer",
    };

    for (uint32_t materialIndex = 0; materialIndex < kMaterialCount; ++materialIndex)
    {
        const DemoRenderUtils::LoadedImage textureImage =
            DemoRenderUtils::LoadTextureFileRGBA8(texturePaths[materialIndex], "MultiObjectTexturedScene");

        DemoRenderUtils::CreateRGBA8Texture2DWithView(device,
                                                      textureImage.m_Width,
                                                      textureImage.m_Height,
                                                      kTextureDebugNames[materialIndex],
                                                      kTextureViewDebugNames[materialIndex],
                                                      m_Textures[materialIndex],
                                                      m_TextureViews[materialIndex]);

        m_TextureUploadBuffers[materialIndex] =
            RHIUpload::CreateUploadBuffer(device,
                                          textureImage.m_Pixels.data(),
                                          static_cast<uint64_t>(textureImage.m_Pixels.size()),
                                          kTextureUploadDebugNames[materialIndex]);
        m_TextureUploadPending[materialIndex] = true;
    }

    m_Sampler = DemoRenderUtils::CreateLinearRepeatSampler(device, "MultiObjectTexturedScene.LinearRepeatSampler");

    CreateDepthResources();

    Renderer::ForwardRendererDesc rendererDesc;
    rendererDesc.m_ShaderPath = rootPath / "Project" / "Shaders" / "DemoTextured.slang";
    rendererDesc.m_ColorFormat = app.GetSwapchain().GetFormat();
    rendererDesc.m_DepthFormat = Format::D32_SFLOAT;
    rendererDesc.m_VertexStride = static_cast<uint32_t>(sizeof(TexturedVertex));
    rendererDesc.m_PositionOffset = static_cast<uint32_t>(offsetof(TexturedVertex, m_Position));
    rendererDesc.m_UVOffset = static_cast<uint32_t>(offsetof(TexturedVertex, m_UV));
    rendererDesc.m_DebugName = "MultiObjectTexturedScene";
    m_Renderer.Initialize(device, rendererDesc);

    for (Scope<ResourceSet>& materialSet : m_MaterialSets)
        materialSet = m_Renderer.CreateMaterialSet(device);
    for (Scope<ResourceSet>& objectSet : m_ObjectSets)
        objectSet = m_Renderer.CreateObjectSet(device);

    m_Mesh.m_VertexBuffer = m_VertexBuffer.get();
    m_Mesh.m_IndexBuffer = m_IndexBuffer.get();
    m_Mesh.m_IndexType = IndexType::UInt16;
    m_Mesh.m_IndexCount = 36;

    static const std::array<Math::Vec4, kMaterialCount> kMaterialBaseColors = {
        Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
        Math::Vec4(0.90f, 0.95f, 1.0f, 1.0f),
    };
    for (uint32_t materialIndex = 0; materialIndex < kMaterialCount; ++materialIndex)
    {
        m_Materials[materialIndex].m_BaseColor = kMaterialBaseColors[materialIndex];
        m_Materials[materialIndex].m_AlbedoTextureView = m_TextureViews[materialIndex].get();
        m_Materials[materialIndex].m_AlbedoSampler = m_Sampler.get();
        m_Materials[materialIndex].m_ResourceSet = m_MaterialSets[materialIndex].get();
    }

    static const std::array<uint32_t, kObjectCount> kObjectMaterialIndices = {0, 1, 0};

    for (uint32_t objectIndex = 0; objectIndex < kObjectCount; ++objectIndex)
    {
        m_RenderObjects[objectIndex].m_Mesh = &m_Mesh;
        m_RenderObjects[objectIndex].m_Material = &m_Materials[kObjectMaterialIndices[objectIndex]];
        m_RenderObjects[objectIndex].m_ObjectSet = m_ObjectSets[objectIndex].get();
    }
    UpdateSceneParameters();
#endif
}

void MultiObjectTexturedSceneDemo::CreateDepthResources()
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
    depthDesc.m_DebugName = "MultiObjectTexturedScene.Depth";
    m_DepthTexture = device.CreateTexture(depthDesc);

    TextureViewDesc depthViewDesc;
    depthViewDesc.m_Type = TextureType::Tex2D;
    depthViewDesc.m_Format = depthDesc.m_Format;
    depthViewDesc.m_Aspect = TextureAspect::Depth;
    m_DepthView = device.CreateTextureView(m_DepthTexture.get(), depthViewDesc);
#endif
}

void MultiObjectTexturedSceneDemo::UploadPendingResources(CommandList& commandList,
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

    for (uint32_t materialIndex = 0; materialIndex < kMaterialCount; ++materialIndex)
    {
        if (!m_TextureUploadPending[materialIndex])
            continue;

        RHIUpload::UploadFullTexture(commandList,
                                     resourceStateTracker,
                                     m_TextureUploadBuffers[materialIndex].get(),
                                     m_Textures[materialIndex].get(),
                                     4u,
                                     TextureState::ShaderRead);
        m_TextureUploadPending[materialIndex] = false;
    }
#endif
}

void MultiObjectTexturedSceneDemo::UpdateSceneParameters()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    RTRLAB_ASSERT_MSG(m_Renderer.IsInitialized() && m_RenderObjects.front().m_Mesh != nullptr &&
                          m_RenderObjects.front().m_Material != nullptr &&
                          m_RenderObjects.front().m_ObjectSet != nullptr,
                      "MultiObjectTexturedScene parameter updates require initialized shader resources.");

    Renderer::FrameGlobals frameGlobals;
    frameGlobals.m_ViewProjection = DemoRenderUtils::BuildOrbitViewProjection(m_ViewportWidth, m_ViewportHeight);
    frameGlobals.m_Tint = Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    frameGlobals.m_Time = m_ElapsedSeconds;

    static const std::array<Math::Vec3, kObjectCount> kObjectPositions = {
        Math::Vec3(-1.25f, -0.15f, 0.0f),
        Math::Vec3(0.0f, 0.25f, -0.55f),
        Math::Vec3(1.25f, -0.20f, 0.2f),
    };
    static const std::array<float, kObjectCount> kRotationSpeeds = {0.55f, 0.85f, -0.65f};

    m_Renderer.UpdateFrameGlobals(frameGlobals);
    for (const Renderer::Material& material : m_Materials)
        m_Renderer.UpdateMaterial(material);

    for (uint32_t objectIndex = 0; objectIndex < kObjectCount; ++objectIndex)
    {
        Renderer::RenderObject& object = m_RenderObjects[objectIndex];
        const float rotation = m_ElapsedSeconds * kRotationSpeeds[objectIndex];
        object.m_Model = Math::Translate(Math::Mat4::Identity(), kObjectPositions[objectIndex]) *
                         Math::Rotate(Math::Mat4::Identity(), rotation, Math::Vec3(0.0f, 1.0f, 0.0f)) *
                         Math::Rotate(Math::Mat4::Identity(), rotation * 0.6f, Math::Vec3(1.0f, 0.0f, 0.0f));
        m_Renderer.UpdateRenderObject(object);
    }
#endif
}

void MultiObjectTexturedSceneDemo::ForgetTrackedResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    ResourceStateTracker& resourceStateTracker = Application::Get().GetResourceStateTracker();
    resourceStateTracker.Forget(m_DepthTexture.get());
    for (const Scope<Texture>& texture : m_Textures)
        resourceStateTracker.Forget(texture.get());
    for (const Scope<Buffer>& textureUploadBuffer : m_TextureUploadBuffers)
        resourceStateTracker.Forget(textureUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexUploadBuffer.get());
    resourceStateTracker.Forget(m_VertexUploadBuffer.get());
    resourceStateTracker.Forget(m_IndexBuffer.get());
    resourceStateTracker.Forget(m_VertexBuffer.get());
#endif
}
