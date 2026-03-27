#include "TexturedQuad.h"

#include <imgui.h>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/Buffers.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IIndexBuffer.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"
#include "graphics/interfaces/IVertexArray.h"
#include "graphics/interfaces/IVertexBuffer.h"

// NDC quad - position (x, y, z) + texcoord (u, v) per vertex.
// clang-format off
static constexpr float kQuadVertices[] = {
    // position          // texcoord
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // bottom-left
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // bottom-right
     0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  // top-right
    -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,  // top-left
};

static constexpr uint32_t kQuadIndices[] = {
    0, 1, 2,  // first triangle
    2, 3, 0,  // second triangle
};
// clang-format on

TexturedQuad::TexturedQuad(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void TexturedQuad::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "TexturedQuad demo attached");

    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);

    // Shader
    m_Shader = GetDevice()->CreateShader("UnlitTextured");
    RTRLAB_ASSERT_MSG(m_Shader, "TexturedQuad demo failed to create UnlitTextured shader");

    // Texture
    auto texturePath = FileSystem::GetAssetPath("textures/Grassy_Square.jpg");
    m_Texture = GetDevice()->CreateTexture2DFromFile(texturePath.string());
    RTRLAB_ASSERT_MSG(m_Texture, "TexturedQuad demo failed to load texture");

    // Vertex buffer with position + texcoord layout
    m_VBO = GetDevice()->CreateVertexBuffer(
        kQuadVertices, sizeof(kQuadVertices), BufferUsage::StaticDraw);
    m_VBO->SetLayout({
        {ShaderDataType::Float3, "a_Position"},
        {ShaderDataType::Float2, "a_TexCoord"},
    });

    // Index buffer
    m_IBO = GetDevice()->CreateIndexBuffer(kQuadIndices, 6);

    // Vertex array
    m_VAO = GetDevice()->CreateVertexArray();
    m_VAO->AddVertexBuffer(m_VBO);
    m_VAO->SetIndexBuffer(m_IBO);
}

void TexturedQuad::OnDetach()
{
    m_VAO.reset();
    m_VBO.reset();
    m_IBO.reset();
    m_Texture.reset();
    m_Shader.reset();
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::Demo, "TexturedQuad demo detached");
}

void TexturedQuad::OnRender()
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
    RenderCommand::SetTexture(1, m_Texture); // binding 1 matches [vk::binding(1, 0)]
    m_VAO->Bind();
    RenderCommand::DrawIndexed(m_VAO);

    RenderCommand::EndRenderPass();
}

void TexturedQuad::OnImGuiRender()
{
    ImGui::Begin("03 - Textured Quad");
    ImGui::TextWrapped(
        "A quad drawn with indexed rendering and a texture.\n"
        "Validates: IBO, ITexture2D, CreateTexture2DFromFile, SetTexture, DrawIndexed.");
    ImGui::End();
}

void TexturedQuad::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
}
