#include "BasicLighting.h"

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Mesh.h"
#include "graphics/MeshFactory.h"
#include "graphics/RenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"

BasicLighting::BasicLighting(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void BasicLighting::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "BasicLighting demo attached");

    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);

    m_Shader = GetDevice()->CreateShader("BasicLit");
    RTRLAB_ASSERT_MSG(m_Shader, "BasicLighting demo failed to create BasicLit shader");

    m_CubeMesh = MeshFactory::CreateCube();

    // Camera
    float aspect = static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight);
    m_Camera.SetPerspective(45.0f, aspect, 0.1f, 100.0f);
    m_Camera.SetPosition({0.0f, 2.0f, 5.0f});
    m_Camera.SetRotation(-90.0f, -15.0f);

    m_CameraController.SetCamera(&m_Camera);
    m_CameraController.SetMoveSpeed(5.0f);
    m_CameraController.SetMouseSensitivity(0.12f);
    m_CameraController.SetScrollSensitivity(2.0f);

    // Input
    m_InputMap.BindAxis("MoveForward", Key::W, Key::S);
    m_InputMap.BindAxis("MoveRight", Key::D, Key::A);
    m_InputMap.BindAxis("MoveUp", Key::E, Key::Q);
    m_InputMap.BindAxis("LookX", InputActionMap::MouseAxis::X);
    m_InputMap.BindAxis("LookY", InputActionMap::MouseAxis::Y);
    m_InputMap.BindAxis("Zoom", InputActionMap::MouseAxis::ScrollY);
    m_InputMap.BindAction("ToggleLookMode", InputSource::FromMouseButton(Mouse::Right));
}

void BasicLighting::OnDetach()
{
    m_CubeMesh.reset();
    m_Shader.reset();
    m_BackBuffer.reset();
    m_InputMap.Clear();
    LOG_INFO_CAT(LogCategory::Demo, "BasicLighting demo detached");
}

void BasicLighting::OnUpdate(double dt)
{
    HandleCameraInput(dt);
}

void BasicLighting::DrawCube(const glm::mat4 &vp, const glm::mat4 &model,
                             const glm::vec3 &albedo, float specPower, float ambient)
{
    const ShaderUniformBlockLayout *blockLayout = m_Shader->GetUniformBlockLayout(0);
    RTRLAB_ASSERT_MSG(blockLayout,
                      "BasicLighting: shader must provide reflected layout for uniform block binding 0.");

    const glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));
    const glm::vec3 lightDirection = glm::normalize(m_LightDirection);
    PackedUniformBlock block(*blockLayout);
    block.WriteRequired("u_ViewProjection", vp);
    block.WriteRequired("u_Model", model);
    block.WriteRequired("u_NormalMatrix", normalMatrix);
    block.WriteRequired("u_CameraPosition", m_Camera.GetPosition());
    block.WriteRequired("u_LightDirection", lightDirection);
    block.WriteRequired("u_LightColor", m_LightColor);
    block.WriteRequired("u_LightIntensity", m_LightIntensity);
    block.WriteRequired("u_Albedo", albedo);
    block.WriteRequired("u_SpecularPower", specPower);
    block.WriteRequired("u_AmbientStrength", ambient);
    m_Shader->SetUniformBlock(0, block.Data(), block.Size());

    RenderCommand::DrawIndexed(m_CubeMesh->GetVertexArray());
}

void BasicLighting::OnRender()
{
    glm::mat4 vp = m_Camera.GetViewProjection();

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
    m_CubeMesh->Bind();

    // Main cube — solid color with Phong shading
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
        DrawCube(vp, model, glm::vec3(0.85f), 32.0f, 0.1f);
    }

    // Second cube — rotated
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.5f, -1.5f));
        model = glm::rotate(model, glm::radians(35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        DrawCube(vp, model, glm::vec3(0.85f), 32.0f, 0.1f);
    }

    // Light indicator — small cube at light direction, fully ambient (appears as solid color)
    {
        glm::vec3 lightPos = -glm::normalize(m_LightDirection) * 3.0f + glm::vec3(0.0f, 1.0f, 0.0f);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), lightPos);
        model = glm::scale(model, glm::vec3(0.15f));
        DrawCube(vp, model, m_LightColor, 1.0f, 1.0f);
    }

    RenderCommand::EndRenderPass();
}

void BasicLighting::OnImGuiRender()
{
    ImGui::Begin("06 - Basic Lighting");
    ImGui::TextWrapped(
        "Blinn-Phong lighting with a single directional light.\n"
        "Validates: normal passing, ambient + diffuse + specular, SetUniformBlock.");

    ImGui::Separator();
    ImGui::Text("Directional Light");
    ImGui::DragFloat3("Direction", &m_LightDirection.x, 0.01f, -1.0f, 1.0f);
    ImGui::ColorEdit3("Color", &m_LightColor.x);
    ImGui::DragFloat("Intensity", &m_LightIntensity, 0.01f, 0.0f, 5.0f);

    ImGui::Separator();
    const auto &pos = m_Camera.GetPosition();
    ImGui::Text("Camera: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);

    ImGui::End();
}

void BasicLighting::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
    m_Camera.SetViewportSize(width, height);
}

void BasicLighting::HandleCameraInput(double dt)
{
    float forward = m_InputMap.GetAxis("MoveForward");
    float right = m_InputMap.GetAxis("MoveRight");
    float up = m_InputMap.GetAxis("MoveUp");

    if (forward > 0.0f)
        m_CameraController.MoveForward(dt);
    if (forward < 0.0f)
        m_CameraController.MoveBackward(dt);
    if (right > 0.0f)
        m_CameraController.MoveRight(dt);
    if (right < 0.0f)
        m_CameraController.MoveLeft(dt);
    if (up > 0.0f)
        m_CameraController.MoveUp(dt);
    if (up < 0.0f)
        m_CameraController.MoveDown(dt);

    if (m_InputMap.IsActionDown("ToggleLookMode"))
    {
        float lookX = m_InputMap.GetAxis("LookX");
        float lookY = m_InputMap.GetAxis("LookY");
        if (lookX != 0.0f || lookY != 0.0f)
            m_CameraController.OnMouseDelta(lookX, -lookY);
    }

    float scroll = m_InputMap.GetAxis("Zoom");
    if (scroll != 0.0f)
        m_CameraController.OnMouseScroll(scroll);
}
