#include "HelloTriangle.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Render/Shader/ShaderCompiler.h"

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_OPENGL) || defined(GLAB_BACKEND_METAL)
namespace
{
struct TriangleVertex
{
    glm::vec2 m_Position;
    glm::vec4 m_Color;
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

    // TRANSITIONAL(M3): The demo still draws directly in clip space without a
    // transform buffer, so preserve a square logical presentation area via the
    // viewport until renderer-owned projection data exists.
    const float squareExtent = static_cast<float>(std::min(framebufferWidth, framebufferHeight));

    DemoViewport viewport;
    viewport.m_Width = squareExtent;
    viewport.m_Height = squareExtent;
    viewport.m_X = (static_cast<float>(framebufferWidth) - squareExtent) * 0.5f;
    viewport.m_Y = (static_cast<float>(framebufferHeight) - squareExtent) * 0.5f;
    return viewport;
}

ShaderCompileRequest BuildHelloTriangleShaderCompileRequest()
{
    ShaderCompileRequest request;

    const std::filesystem::path shaderPath = FileSystem::GetRootPath() / "Project" / "Shaders" / "HelloTriangle.slang";
    const std::string shaderModule = shaderPath.generic_string();

#if defined(GLAB_BACKEND_VULKAN)
    request.m_Targets.push_back({BackendType::Vulkan, MetalCodeFormat::MslSource});
#elif defined(GLAB_BACKEND_OPENGL)
    request.m_Targets.push_back({BackendType::OpenGL, MetalCodeFormat::MslSource});
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
    m_GraphicsPipeline.reset();
    m_VertexInputLayout.reset();
    m_PipelineLayout.reset();
    m_ShaderProgram.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
    LOG_INFO_CAT(LogCategory::k_Demo, "HelloTriangle demo detached");
}

void HelloTriangle::OnRender()
{
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_OPENGL) || defined(GLAB_BACKEND_METAL)
    if (!m_GraphicsPipeline || !m_VertexBuffer || !m_IndexBuffer)
        return;

    CommandList* commandList = Application::Get().GetCurrentCommandList();
    RTRLAB_ASSERT_MSG(commandList != nullptr, "HelloTriangle requires an active command list during OnRender.");
    // NOTE: In the current demo contract, Application owns the outer
    // BeginRendering/EndRendering scope and layers only record draw calls into
    // that active pass. A future render-graph path will move pass ownership out
    // of Application::RenderFrame and make it explicit at a higher level.

    MeshBinding meshBinding;
    meshBinding.m_VertexBuffers = {m_VertexBuffer.get()};
    meshBinding.m_IndexBuffer = m_IndexBuffer.get();
    meshBinding.m_IndexType = IndexType::UInt16;

    const DemoViewport viewport = ComputeAspectPreservingViewport(m_ViewportWidth, m_ViewportHeight);
    commandList->SetViewport(viewport.m_X, viewport.m_Y, viewport.m_Width, viewport.m_Height, 0.0f, 1.0f);
    commandList->SetScissor(0, 0, m_ViewportWidth, m_ViewportHeight);
    commandList->BindGraphicsPipeline(m_GraphicsPipeline.get());
    commandList->BindMesh(meshBinding);
    commandList->DrawIndexed(3, 0, 0);
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
#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_OPENGL) || defined(GLAB_BACKEND_METAL)
    Application& app = Application::Get();
    Device& device = app.GetDevice();

    static constexpr std::array<TriangleVertex, 3> kVertices = {{
        {{0.0f, -0.65f}, {1.0f, 0.25f, 0.25f, 1.0f}},
        {{0.65f, 0.55f}, {0.25f, 1.0f, 0.35f, 1.0f}},
        {{-0.65f, 0.55f}, {0.25f, 0.45f, 1.0f, 1.0f}},
    }};
    static constexpr std::array<uint16_t, 3> kIndices = {0, 1, 2};

    BufferDesc vertexBufferDesc;
    vertexBufferDesc.m_Size = sizeof(kVertices);
    vertexBufferDesc.m_UsageMask = BufferUsage::Vertex;
    vertexBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    vertexBufferDesc.m_DebugName = "HelloTriangle.VertexBuffer";
    m_VertexBuffer = device.CreateBuffer(vertexBufferDesc);
    // TRANSITIONAL(M3): HelloTriangle still uploads directly through Device
    // until renderer-owned staging/upload code exists.
    device.WriteBuffer(m_VertexBuffer.get(), 0, kVertices.data(), sizeof(kVertices));

    BufferDesc indexBufferDesc;
    indexBufferDesc.m_Size = sizeof(kIndices);
    indexBufferDesc.m_UsageMask = BufferUsage::Index;
    indexBufferDesc.m_MemoryUsage = MemoryUsage::CpuToGpu;
    indexBufferDesc.m_DebugName = "HelloTriangle.IndexBuffer";
    m_IndexBuffer = device.CreateBuffer(indexBufferDesc);
    device.WriteBuffer(m_IndexBuffer.get(), 0, kIndices.data(), sizeof(kIndices));

    m_ShaderProgram = device.CreateShaderProgram(BuildHelloTriangleShaderProgramDesc());
    m_PipelineLayout = device.CreatePipelineLayout(m_ShaderProgram->DerivePipelineLayoutDesc());

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
