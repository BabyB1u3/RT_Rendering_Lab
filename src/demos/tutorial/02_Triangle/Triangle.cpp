#include "Triangle.h"

#include <imgui.h>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/Buffers.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/IVertexArray.h"
#include "graphics/interfaces/IVertexBuffer.h"

// NDC triangle — position (x, y, z) + color (r, g, b) per vertex.
// clang-format off
static constexpr float kTriangleVertices[] = {
    // position          // color
    -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // bottom-left  (red)
     0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom-right (green)
     0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // top-center   (blue)
};
// clang-format on

Triangle::Triangle(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void Triangle::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "Triangle demo attached");

    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);

    // Shader
    m_Shader = GetDevice()->CreateShader("FlatColor");
    RTRLAB_ASSERT_MSG(m_Shader, "Triangle demo failed to create FlatColor shader");

    // Vertex buffer with position + color layout
    m_VBO = GetDevice()->CreateVertexBuffer(
        kTriangleVertices, sizeof(kTriangleVertices), BufferUsage::StaticDraw);
    m_VBO->SetLayout({
        {ShaderDataType::Float3, "a_Position"},
        {ShaderDataType::Float3, "a_Color"},
    });

    // Vertex array
    m_VAO = GetDevice()->CreateVertexArray();
    m_VAO->AddVertexBuffer(m_VBO);
}

void Triangle::OnDetach()
{
    m_VAO.reset();
    m_VBO.reset();
    m_Shader.reset();
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::Demo, "Triangle demo detached");
}

void Triangle::OnRender()
{
    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::Clear;
    desc.ClearColor = {0.1f, 0.1f, 0.1f, 1.0f};

    PipelineState pipeline;
    pipeline.DepthTestEnabled = false;
    pipeline.CullFaceEnabled = false;

    RenderCommand::BeginRenderPass(m_BackBuffer, desc);
    RenderCommand::SetViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    RenderCommand::SetPipelineState(pipeline);

    m_Shader->Bind();
    m_VAO->Bind();
    RenderCommand::DrawArrays(0x0004, 0, 3); // GL_TRIANGLES = 0x0004

    RenderCommand::EndRenderPass();
}

void Triangle::OnImGuiRender()
{
    ImGui::Begin("02 - Triangle");
    ImGui::TextWrapped(
        "A single triangle drawn from hand-built NDC vertices.\n"
        "Validates: VBO, VAO, BufferLayout, Shader, DrawArrays.");
    ImGui::End();
}

void Triangle::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
}
