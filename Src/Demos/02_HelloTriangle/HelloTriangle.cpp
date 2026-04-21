#include "HelloTriangle.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <utility>

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Demos/02_HelloTriangle/HelloTriangleShaders.h"

#if defined(GLAB_BACKEND_VULKAN) || defined(GLAB_BACKEND_OPENGL) || defined(GLAB_BACKEND_METAL)
namespace
{
struct TriangleVertex
{
    glm::vec2 m_Position;
    glm::vec4 m_Color;
};

template <size_t N> std::vector<uint8_t> MakeShaderBytes(const uint32_t (&words)[N])
{
    std::vector<uint8_t> bytes(sizeof(words));
    std::memcpy(bytes.data(), words, sizeof(words));
    return bytes;
}

std::vector<uint8_t> MakeShaderBytes(const char* sourceText)
{
    RTRLAB_ASSERT_MSG(sourceText != nullptr, "Shader source text must be valid.");
    const size_t sourceLength = std::strlen(sourceText);
    std::vector<uint8_t> bytes(sourceLength);
    std::memcpy(bytes.data(), sourceText, sourceLength);
    return bytes;
}

CompiledShaderProgramDesc BuildHelloTriangleShaderProgramDesc()
{
    CompiledShaderProgramDesc desc;

#if defined(GLAB_BACKEND_VULKAN)
    CompiledShaderBlob vertexShader;
    vertexShader.m_Backend = BackendType::Vulkan;
    vertexShader.m_Stage = ShaderStage::Vertex;
    vertexShader.m_Code = MakeShaderBytes(kHelloTriangleVertexSpirv);
    desc.m_Blobs.push_back(std::move(vertexShader));

    CompiledShaderBlob fragmentShader;
    fragmentShader.m_Backend = BackendType::Vulkan;
    fragmentShader.m_Stage = ShaderStage::Fragment;
    fragmentShader.m_Code = MakeShaderBytes(kHelloTriangleFragmentSpirv);
    desc.m_Blobs.push_back(std::move(fragmentShader));
#elif defined(GLAB_BACKEND_OPENGL)
    CompiledShaderBlob vertexShader;
    vertexShader.m_Backend = BackendType::OpenGL;
    vertexShader.m_Stage = ShaderStage::Vertex;
    vertexShader.m_Code = MakeShaderBytes(kHelloTriangleVertexOpenGL);
    desc.m_Blobs.push_back(std::move(vertexShader));

    CompiledShaderBlob fragmentShader;
    fragmentShader.m_Backend = BackendType::OpenGL;
    fragmentShader.m_Stage = ShaderStage::Fragment;
    fragmentShader.m_Code = MakeShaderBytes(kHelloTriangleFragmentOpenGL);
    desc.m_Blobs.push_back(std::move(fragmentShader));
#elif defined(GLAB_BACKEND_METAL)
    CompiledShaderBlob vertexShader;
    vertexShader.m_Backend = BackendType::Metal;
    vertexShader.m_Stage = ShaderStage::Vertex;
    vertexShader.m_MetalCodeFormat = MetalCodeFormat::MslSource;
    vertexShader.m_Code = MakeShaderBytes(kHelloTriangleMetalSource);
    desc.m_Blobs.push_back(std::move(vertexShader));

    CompiledShaderBlob fragmentShader;
    fragmentShader.m_Backend = BackendType::Metal;
    fragmentShader.m_Stage = ShaderStage::Fragment;
    fragmentShader.m_MetalCodeFormat = MetalCodeFormat::MslSource;
    fragmentShader.m_Code = MakeShaderBytes(kHelloTriangleMetalSource);
    desc.m_Blobs.push_back(std::move(fragmentShader));
#endif

    return desc;
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

    MeshBinding meshBinding;
    meshBinding.m_VertexBuffers = {m_VertexBuffer.get()};
    meshBinding.m_IndexBuffer = m_IndexBuffer.get();
    meshBinding.m_IndexType = IndexType::UInt16;

    commandList->SetViewport(
        0.0f, 0.0f, static_cast<float>(m_ViewportWidth), static_cast<float>(m_ViewportHeight), 0.0f, 1.0f);
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
