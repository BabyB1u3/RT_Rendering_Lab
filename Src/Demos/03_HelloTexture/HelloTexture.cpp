#include "HelloTexture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <stb_image.h>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Util/Math.h"
#include "Render/Shader/ShaderCompiler.h"
#include "Render/Shader/ShaderParameterWriter.h"

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
namespace HelloTextureDemo
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

struct LoadedImage
{
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    std::vector<uint8_t> m_Pixels;
};

DemoViewport ComputeAspectPreservingViewport(uint32_t framebufferWidth, uint32_t framebufferHeight)
{
    RTRLAB_ASSERT_MSG(framebufferWidth > 0 && framebufferHeight > 0,
                      "HelloTexture requires a non-zero framebuffer size.");

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
                   "HelloTexture failed to find reflected binding '{}' in the PipelineLayout.",
                   bindingName);
    return it->m_SetIndex;
}

LoadedImage LoadTextureFileRGBA8(const std::filesystem::path& texturePath)
{
    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    stbi_uc* pixels = stbi_load(texturePath.string().c_str(), &width, &height, &sourceChannels, 4);
    RTRLAB_ASSERTF(pixels != nullptr,
                   "HelloTexture failed to load texture '{}': {}",
                   texturePath.string(),
                   stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown stb_image error");
    RTRLAB_ASSERT_MSG(width > 0 && height > 0, "HelloTexture loaded an invalid texture extent.");

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    LoadedImage image;
    image.m_Width = static_cast<uint32_t>(width);
    image.m_Height = static_cast<uint32_t>(height);
    image.m_Pixels.assign(pixels, pixels + pixelCount);
    stbi_image_free(pixels);
    return image;
}

ShaderCompileRequest BuildHelloTextureShaderCompileRequest()
{
    ShaderCompileRequest request;

    const std::filesystem::path shaderPath = FileSystem::GetRootPath() / "Project" / "Shaders" / "HelloTexture.slang";
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

CompiledShaderProgramDesc BuildHelloTextureShaderProgramDesc()
{
    Scope<ShaderCompiler> shaderCompiler = CreateShaderCompiler();
    RTRLAB_ASSERT_MSG(shaderCompiler != nullptr, "HelloTexture requires a valid ShaderCompiler instance.");

    ShaderCompileResult compileResult = shaderCompiler->CompileProgram(BuildHelloTextureShaderCompileRequest());
    RTRLAB_ASSERTF(compileResult.m_Succeeded,
                   "HelloTexture failed to compile its Slang shader program: {}",
                   compileResult.m_ErrorMessage);
    return std::move(compileResult.m_Program);
}
} // namespace HelloTextureDemo
#endif

HelloTexture::HelloTexture(uint32_t width, uint32_t height) : m_ViewportWidth(width), m_ViewportHeight(height) {}

void HelloTexture::OnAttach()
{
    CreateTextureDemoResources();
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloTexture demo attached");
}

void HelloTexture::OnDetach()
{
    ForgetTrackedResources();
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_TextureSet.reset();
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
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloTexture demo detached");
}

void HelloTexture::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer || !m_TextureSet)
        return;

    Application& app = Application::Get();
    CommandList* commandList = app.GetCurrentCommandList();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "HelloTexture requires an active command list during OnRender.");
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    TextureView* swapchainImageView = app.GetCurrentSwapchainImageView();
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr && swapchainImageView != nullptr,
                      "HelloTexture requires the application to expose the active swapchain backbuffer.");

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

    MeshBinding meshBinding;
    meshBinding.m_VertexBuffers = {m_VertexBuffer.get()};
    meshBinding.m_IndexBuffer = m_IndexBuffer.get();
    meshBinding.m_IndexType = IndexType::UInt16;

    const HelloTextureDemo::DemoViewport viewport =
        HelloTextureDemo::ComputeAspectPreservingViewport(m_ViewportWidth, m_ViewportHeight);
    commandList->SetViewport(viewport.m_X, viewport.m_Y, viewport.m_Width, viewport.m_Height, 0.0f, 1.0f);
    commandList->SetScissor(0, 0, m_ViewportWidth, m_ViewportHeight);
    commandList->BindGraphicsPipeline(m_GraphicsPipeline.get());
    commandList->BindResourceSet(m_TextureSetIndex, m_TextureSet.get());
    commandList->BindMesh(meshBinding);
    commandList->DrawIndexed(6, 0, 0);
    commandList->EndRendering();
#endif
}

void HelloTexture::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
}

void HelloTexture::CreateTextureDemoResources()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    static const std::array<HelloTextureDemo::TexturedVertex, 4> kVertices = {{
        {{-0.75f, -0.75f}, {0.0f, 1.0f}},
        {{0.75f, -0.75f}, {1.0f, 1.0f}},
        {{0.75f, 0.75f}, {1.0f, 0.0f}},
        {{-0.75f, 0.75f}, {0.0f, 0.0f}},
    }};
    static constexpr std::array<uint16_t, 6> kIndices = {0, 1, 2, 0, 2, 3};

    BufferDesc vertexBufferDesc;
    vertexBufferDesc.m_Size = sizeof(kVertices);
    vertexBufferDesc.m_UsageMask = BufferUsage::Vertex | BufferUsage::CopyDst;
    vertexBufferDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    vertexBufferDesc.m_DebugName = "HelloTexture.VertexBuffer";
    m_VertexBuffer = device.CreateBuffer(vertexBufferDesc);

    BufferDesc vertexUploadBufferDesc;
    vertexUploadBufferDesc.m_Size = sizeof(kVertices);
    vertexUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    vertexUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    vertexUploadBufferDesc.m_DebugName = "HelloTexture.VertexUploadBuffer";
    m_VertexUploadBuffer = device.CreateBuffer(vertexUploadBufferDesc);
    device.WriteBuffer(m_VertexUploadBuffer.get(), 0, kVertices.data(), sizeof(kVertices));

    BufferDesc indexBufferDesc;
    indexBufferDesc.m_Size = sizeof(kIndices);
    indexBufferDesc.m_UsageMask = BufferUsage::Index | BufferUsage::CopyDst;
    indexBufferDesc.m_MemoryUsage = MemoryUsage::GpuOnly;
    indexBufferDesc.m_DebugName = "HelloTexture.IndexBuffer";
    m_IndexBuffer = device.CreateBuffer(indexBufferDesc);

    BufferDesc indexUploadBufferDesc;
    indexUploadBufferDesc.m_Size = sizeof(kIndices);
    indexUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    indexUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    indexUploadBufferDesc.m_DebugName = "HelloTexture.IndexUploadBuffer";
    m_IndexUploadBuffer = device.CreateBuffer(indexUploadBufferDesc);
    device.WriteBuffer(m_IndexUploadBuffer.get(), 0, kIndices.data(), sizeof(kIndices));
    m_GeometryUploadPending = true;

    const std::filesystem::path texturePath = FileSystem::GetRootPath() / "Project" / "Textures" / "Grassy_Square.jpg";
    const HelloTextureDemo::LoadedImage loadedImage = HelloTextureDemo::LoadTextureFileRGBA8(texturePath);

    TextureDesc textureDesc;
    textureDesc.m_Type = TextureType::Tex2D;
    textureDesc.m_Format = Format::RGBA8_UNORM;
    textureDesc.m_Extent = {loadedImage.m_Width, loadedImage.m_Height, 1};
    textureDesc.m_MipLevels = 1;
    textureDesc.m_ArrayLayers = 1;
    textureDesc.m_UsageMask = TextureUsage::Sampled | TextureUsage::CopyDst;
    textureDesc.m_DebugName = "HelloTexture.ColorTexture";
    m_Texture = device.CreateTexture(textureDesc);

    TextureViewDesc textureViewDesc;
    textureViewDesc.m_Type = TextureType::Tex2D;
    textureViewDesc.m_Format = textureDesc.m_Format;
    textureViewDesc.m_Aspect = TextureAspect::Color;
    textureViewDesc.m_BaseMipLevel = 0;
    textureViewDesc.m_MipLevelCount = 1;
    textureViewDesc.m_BaseArrayLayer = 0;
    textureViewDesc.m_ArrayLayerCount = 1;
    m_TextureView = device.CreateTextureView(m_Texture.get(), textureViewDesc);

    SamplerDesc samplerDesc;
    samplerDesc.m_MinFilter = FilterMode::Linear;
    samplerDesc.m_MagFilter = FilterMode::Linear;
    samplerDesc.m_MipFilter = MipFilterMode::Linear;
    samplerDesc.m_AddressU = AddressMode::Repeat;
    samplerDesc.m_AddressV = AddressMode::Repeat;
    samplerDesc.m_AddressW = AddressMode::Repeat;
    m_Sampler = device.CreateSampler(samplerDesc);

    BufferDesc textureUploadBufferDesc;
    textureUploadBufferDesc.m_Size = static_cast<uint64_t>(loadedImage.m_Pixels.size());
    textureUploadBufferDesc.m_UsageMask = BufferUsage::CopySrc;
    textureUploadBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    textureUploadBufferDesc.m_DebugName = "HelloTexture.TextureUploadBuffer";
    m_TextureUploadBuffer = device.CreateBuffer(textureUploadBufferDesc);
    device.WriteBuffer(m_TextureUploadBuffer.get(),
                       0,
                       loadedImage.m_Pixels.data(),
                       static_cast<uint64_t>(loadedImage.m_Pixels.size()));
    m_TextureUploadPending = true;

    m_ShaderProgram = device.CreateShaderProgram(HelloTextureDemo::BuildHelloTextureShaderProgramDesc());
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());
    const PipelineLayoutDesc& pipelineLayoutDesc = m_PipelineLayout->GetDesc();
    m_TextureSetIndex = HelloTextureDemo::FindRequiredSetIndex(pipelineLayoutDesc, "gAlbedo");
    m_TextureSet = device.CreateResourceSet(m_PipelineLayout.get(), m_TextureSetIndex);

    ShaderParameterWriter parameterWriter(m_ShaderProgram->GetReflection());
    parameterWriter.SetTextureView(*m_TextureSet, "gAlbedo", m_TextureView.get());
    parameterWriter.SetSampler(*m_TextureSet, "gLinearSampler", m_Sampler.get());

    VertexInputLayoutDesc vertexInputLayoutDesc;
    vertexInputLayoutDesc.m_Buffers = {{static_cast<uint32_t>(sizeof(HelloTextureDemo::TexturedVertex)), false}};
    vertexInputLayoutDesc.m_Attributes = {
        {0u, Format::RG32F, static_cast<uint32_t>(offsetof(HelloTextureDemo::TexturedVertex, m_Position)), 0u},
        {1u, Format::RG32F, static_cast<uint32_t>(offsetof(HelloTextureDemo::TexturedVertex, m_UV)), 0u},
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

void HelloTexture::UploadPendingResources(CommandList& commandList, ResourceStateTracker& resourceStateTracker)
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_METAL)
    if (m_GeometryUploadPending)
    {
        RTRLAB_ASSERT_MSG(m_VertexUploadBuffer != nullptr && m_VertexBuffer != nullptr,
                          "HelloTexture geometry upload requires both vertex staging and GPU buffers.");
        RTRLAB_ASSERT_MSG(m_IndexUploadBuffer != nullptr && m_IndexBuffer != nullptr,
                          "HelloTexture geometry upload requires both index staging and GPU buffers.");

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
    }

    if (m_TextureUploadPending)
    {
        RTRLAB_ASSERT_MSG(m_TextureUploadBuffer != nullptr && m_Texture != nullptr,
                          "HelloTexture texture upload requires both staging and destination textures.");

        resourceStateTracker.Transition(m_TextureUploadBuffer.get(), BufferState::CopySource);
        resourceStateTracker.Transition(m_Texture.get(), TextureState::CopyDest);
        resourceStateTracker.FlushBarriers(&commandList);

        const TextureDesc& textureDesc = m_Texture->GetDesc();
        const std::array<BufferTextureCopyRegion, 1> copyRegions = {{
            {0,
             textureDesc.m_Extent.m_Width * 4u,
             textureDesc.m_Extent.m_Height,
             TextureAspect::Color,
             0,
             0,
             1,
             {0, 0, 0},
             textureDesc.m_Extent},
        }};
        commandList.CopyBufferToTexture(m_TextureUploadBuffer.get(), m_Texture.get(), copyRegions);

        resourceStateTracker.Transition(m_Texture.get(), TextureState::ShaderRead);
        resourceStateTracker.FlushBarriers(&commandList);

        m_TextureUploadPending = false;
    }
#endif
}

void HelloTexture::ForgetTrackedResources()
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
