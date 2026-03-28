#include "TransformedCube.h"

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Mesh.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"

namespace
{
    constexpr ShaderBindingPoint kUnlitTextureBinding{0, 1};
}

TransformedCube::TransformedCube(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void TransformedCube::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "TransformedCube demo attached");

    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);

    // Shader
    m_Shader = GetDevice()->CreateShader("UnlitTransformed");
    RTRLAB_ASSERT_MSG(m_Shader, "TransformedCube demo failed to create UnlitTransformed shader");

    // Texture
    auto texturePath = FileSystem::GetAssetPath("textures/Grassy_Square.jpg");
    m_Texture = GetDevice()->CreateTexture2DFromFile(texturePath.string());
    RTRLAB_ASSERT_MSG(m_Texture, "TransformedCube demo failed to load texture");

    // Cube mesh from MeshFactory
    m_CubeMesh = MeshFactory::CreateCube();
}

void TransformedCube::OnDetach()
{
    m_CubeMesh.reset();
    m_Texture.reset();
    m_Shader.reset();
    m_BackBuffer.reset();
    LOG_INFO_CAT(LogCategory::Demo, "TransformedCube demo detached");
}

void TransformedCube::OnUpdate(double dt)
{
    m_RotationY += m_RotationSpeed * static_cast<float>(dt);
    m_RotationX += m_RotationSpeed * 0.7f * static_cast<float>(dt);

    // Keep angles in [0, 360)
    if (m_RotationY >= 360.0f)
        m_RotationY -= 360.0f;
    if (m_RotationX >= 360.0f)
        m_RotationX -= 360.0f;
}

void TransformedCube::OnRender()
{
    // --- Model matrix: rotate around Y and X axes ---
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(m_RotationY), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_RotationX), glm::vec3(1.0f, 0.0f, 0.0f));

    // --- View matrix: camera looking at origin from a fixed position ---
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 3.0f), // eye
        glm::vec3(0.0f, 0.0f, 0.0f), // center
        glm::vec3(0.0f, 1.0f, 0.0f)  // up
    );

    // --- Projection matrix: perspective ---
    float aspect = static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    glm::mat4 mvp = projection * view * model;

    // --- Render ---
    RenderPassDescriptor desc;
    desc.ColorLoadAction = LoadAction::Clear;
    desc.ClearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    desc.DepthLoadAction = LoadAction::Clear;

    PipelineState pipeline;
    pipeline.DepthTestEnabled = true;
    pipeline.DepthWriteEnabled = true;
    pipeline.CullFaceEnabled = true;

    RenderCommand::BeginRenderPass(m_BackBuffer, desc);
    RenderCommand::SetViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    RenderCommand::SetPipelineState(pipeline);

    m_Shader->Bind();
    m_Shader->SetUniformBlock(0, &mvp, sizeof(mvp));
    m_Shader->BindTexture(kUnlitTextureBinding, m_Texture);
    m_CubeMesh->Bind();
    RenderCommand::DrawIndexed(m_CubeMesh->GetVertexArray());

    RenderCommand::EndRenderPass();
}

void TransformedCube::OnImGuiRender()
{
    ImGui::Begin("04 - Transformed Cube");
    ImGui::TextWrapped(
        "A rotating textured cube with perspective projection.\n"
        "Validates: MVP uniform (SetUniformBlock), depth testing, MeshFactory.");
    ImGui::SliderFloat("Rotation Speed", &m_RotationSpeed, 0.0f, 180.0f, "%.0f deg/s");
    ImGui::End();
}

void TransformedCube::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
}
