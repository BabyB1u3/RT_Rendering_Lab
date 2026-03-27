#include "CameraDemo.h"

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

// Positions and rotations for cubes scattered in the scene.
// clang-format off
static const struct { glm::vec3 pos; glm::vec3 rot; } kCubes[] = {
    {{ 0.0f,  0.0f,  0.0f}, {  0.0f,   0.0f,  0.0f}},
    {{ 2.0f,  0.5f, -1.5f}, { 20.0f,  35.0f,  0.0f}},
    {{-1.5f,  0.8f, -2.5f}, {-15.0f, -20.0f, 10.0f}},
    {{ 1.0f, -0.5f, -4.0f}, { 45.0f,  60.0f,  0.0f}},
    {{-2.5f,  0.0f, -3.0f}, {  0.0f,  50.0f, 30.0f}},
};
// clang-format on
static constexpr int kCubeCount = sizeof(kCubes) / sizeof(kCubes[0]);

CameraDemo::CameraDemo(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height)
{
}

void CameraDemo::OnAttach()
{
    LOG_INFO_CAT(LogCategory::Demo, "CameraDemo attached");

    m_BackBuffer = GetDevice()->CreateRenderTargetBackBuffer(m_ViewportWidth, m_ViewportHeight);

    // Shader (reuse UnlitTransformed from Tutorial 04)
    m_Shader = GetDevice()->CreateShader("UnlitTransformed");
    RTRLAB_ASSERT_MSG(m_Shader, "CameraDemo failed to create UnlitTransformed shader");

    // Texture
    auto texturePath = FileSystem::GetAssetPath("textures/Grassy_Square.jpg");
    m_Texture = GetDevice()->CreateTexture2DFromFile(texturePath.string());
    RTRLAB_ASSERT_MSG(m_Texture, "CameraDemo failed to load texture");

    // Cube mesh
    m_CubeMesh = MeshFactory::CreateCube();

    // Camera
    float aspect = static_cast<float>(m_ViewportWidth) / static_cast<float>(m_ViewportHeight);
    m_Camera.SetPerspective(45.0f, aspect, 0.1f, 100.0f);
    m_Camera.SetPosition({0.0f, 1.0f, 5.0f});
    m_Camera.SetRotation(-90.0f, -10.0f);

    // Controller
    m_CameraController.SetCamera(&m_Camera);
    m_CameraController.SetMoveSpeed(5.0f);
    m_CameraController.SetMouseSensitivity(0.12f);
    m_CameraController.SetScrollSensitivity(2.0f);

    // Input bindings
    m_InputMap.BindAxis("MoveForward", Key::W, Key::S);
    m_InputMap.BindAxis("MoveRight", Key::D, Key::A);
    m_InputMap.BindAxis("MoveUp", Key::E, Key::Q);
    m_InputMap.BindAxis("LookX", InputActionMap::MouseAxis::X);
    m_InputMap.BindAxis("LookY", InputActionMap::MouseAxis::Y);
    m_InputMap.BindAxis("Zoom", InputActionMap::MouseAxis::ScrollY);
    m_InputMap.BindAction("ToggleLookMode", InputSource::FromMouseButton(Mouse::Right));
}

void CameraDemo::OnDetach()
{
    m_CubeMesh.reset();
    m_Texture.reset();
    m_Shader.reset();
    m_BackBuffer.reset();
    m_InputMap.Clear();
    LOG_INFO_CAT(LogCategory::Demo, "CameraDemo detached");
}

void CameraDemo::OnUpdate(double dt)
{
    HandleCameraInput(dt);
}

void CameraDemo::OnRender()
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
    RenderCommand::SetTexture(1, m_Texture);
    m_CubeMesh->Bind();

    // Draw each cube with its own model matrix
    for (int i = 0; i < kCubeCount; ++i)
    {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), kCubes[i].pos);
        model = glm::rotate(model, glm::radians(kCubes[i].rot.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(kCubes[i].rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(kCubes[i].rot.z), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 mvp = vp * model;
        m_Shader->SetUniformBlock(0, &mvp, sizeof(mvp));
        RenderCommand::DrawIndexed(m_CubeMesh->GetVertexArray());
    }

    RenderCommand::EndRenderPass();
}

void CameraDemo::OnImGuiRender()
{
    ImGui::Begin("05 - Camera");
    ImGui::TextWrapped(
        "WASD to move, QE up/down, right-click + drag to look, scroll to zoom.\n"
        "Validates: Camera, DebugCameraController, InputActionMap, per-draw MVP.");

    const auto &pos = m_Camera.GetPosition();
    ImGui::Text("Position: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
    ImGui::Text("Yaw: %.1f  Pitch: %.1f", m_Camera.GetYaw(), m_Camera.GetPitch());
    ImGui::Text("FOV: %.1f", m_Camera.GetVerticalFovDegrees());

    float speed = m_CameraController.GetMoveSpeed();
    if (ImGui::SliderFloat("Move Speed", &speed, 1.0f, 20.0f))
        m_CameraController.SetMoveSpeed(speed);

    ImGui::End();
}

void CameraDemo::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_BackBuffer->Resize(width, height);
    m_Camera.SetViewportSize(width, height);
}

void CameraDemo::HandleCameraInput(double dt)
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

    // Right-click to look around
    if (m_InputMap.IsActionDown("ToggleLookMode"))
    {
        float lookX = m_InputMap.GetAxis("LookX");
        float lookY = m_InputMap.GetAxis("LookY");
        if (lookX != 0.0f || lookY != 0.0f)
            m_CameraController.OnMouseDelta(lookX, -lookY);
    }

    // Scroll to zoom (adjust FOV)
    float scroll = m_InputMap.GetAxis("Zoom");
    if (scroll != 0.0f)
        m_CameraController.OnMouseScroll(scroll);
}
