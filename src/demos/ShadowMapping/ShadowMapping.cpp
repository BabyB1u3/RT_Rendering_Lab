#include "ShadowMapping.h"

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

#include "core/Input.h"
#include "core/KeyCode.h"
#include "core/Logger.h"
#include "core/MouseCode.h"
#include "graphics/Material.h"
#include "graphics/MeshFactory.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "renderer/RenderItem.h"

ShadowMapping::ShadowMapping(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height),
      m_Camera(45.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f),
      m_CameraController(&m_Camera)
{
}

void ShadowMapping::OnAttach()
{
    LOG_INFO("ShadowMapping demo attached");
    m_Renderer = CreateScope<SceneRenderer>(m_ViewportWidth, m_ViewportHeight);

    m_Camera.SetPosition({0.0f, 2.5f, 6.0f});
    m_Camera.SetRotation(-90.0f, -20.0f);

    m_CameraController.SetMoveSpeed(5.0f);
    m_CameraController.SetMouseSensitivity(0.12f);
    m_CameraController.SetScrollSensitivity(2.0f);

    m_CubeMesh = MeshFactory::CreateCube();
    m_PlaneMesh = MeshFactory::CreatePlane();

    TextureSpecification whiteSpec;
    whiteSpec.Width = 1;
    whiteSpec.Height = 1;
    whiteSpec.Format = TextureFormat::RGBA8;
    m_WhiteTexture = Texture2D::Create(whiteSpec);

    const uint32_t whitePixel = 0xffffffffu;
    m_WhiteTexture->SetData(&whitePixel);

    m_DefaultMaterial = CreateRef<Material>();
    m_DefaultMaterial->SetTexture(TextureSlot::Albedo, m_WhiteTexture);
    m_DefaultMaterial->SetVec3("u_Albedo", glm::vec3(0.85f));
    m_DefaultMaterial->SetFloat("u_SpecularPower", 32.0f);
    m_DefaultMaterial->SetFloat("u_AmbientStrength", 0.15f);

    BuildScene();
}

void ShadowMapping::OnDetach()
{
    LOG_INFO("ShadowMapping demo detached");
    m_Scene.RenderItems.clear();
    m_DefaultMaterial.reset();
    m_WhiteTexture.reset();
    m_CubeMesh.reset();
    m_PlaneMesh.reset();
    m_Renderer.reset();
}

void ShadowMapping::OnUpdate(double dt)
{
    HandleCameraInput(dt);

    if (Input::IsKeyPressed(Key::D1))
        m_OutputMode = SceneRendererOutput::FinalColor;

    if (Input::IsKeyPressed(Key::D2))
        m_OutputMode = SceneRendererOutput::ShadowMap;
}

void ShadowMapping::OnRender()
{
    if (!m_Renderer)
        return;

    m_Renderer->SetOutputMode(m_OutputMode);
    m_Renderer->Render(m_Scene, m_Camera);
}

void ShadowMapping::OnImGuiRender()
{
    ImGui::Begin("Shadow Mapping");

    // Output mode
    int mode = static_cast<int>(m_OutputMode);
    ImGui::Text("Output Mode");
    ImGui::RadioButton("Final Color", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Shadow Map", &mode, 1);
    m_OutputMode = static_cast<SceneRendererOutput>(mode);

    ImGui::Separator();

    // Light direction
    ImGui::Text("Directional Light");
    ImGui::DragFloat3("Direction", &m_Scene.MainDirectionalLight.Direction.x, 0.01f, -1.0f, 1.0f);
    m_Scene.MainDirectionalLight.Direction = glm::normalize(m_Scene.MainDirectionalLight.Direction);
    ImGui::ColorEdit3("Light Color", &m_Scene.MainDirectionalLight.Color.x);
    ImGui::DragFloat("Intensity", &m_Scene.MainDirectionalLight.Intensity, 0.01f, 0.0f, 5.0f);

    ImGui::Separator();

    // Light projection (renderer spec)
    if (m_Renderer)
    {
        auto &spec = m_Renderer->GetSpecification();
        if (ImGui::CollapsingHeader("Light Projection"))
        {
            ImGui::DragFloat("Distance", &spec.LightDistance, 0.1f, 1.0f, 50.0f);
            ImGui::DragFloat("Ortho Size", &spec.LightOrthoSize, 0.1f, 1.0f, 50.0f);
            ImGui::DragFloat("Near Plane", &spec.LightNearPlane, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("Far Plane", &spec.LightFarPlane, 0.1f, 1.0f, 100.0f);
        }
    }

    ImGui::End();
}

void ShadowMapping::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;

    m_Camera.SetViewportSize(width, height);

    if (m_Renderer)
        m_Renderer->Resize(width, height);
}

void ShadowMapping::BuildScene()
{
    m_Scene.RenderItems.clear();

    m_Scene.MainDirectionalLight.Direction = glm::normalize(glm::vec3(-0.8f, -1.0f, -0.4f));
    m_Scene.MainDirectionalLight.Color = glm::vec3(1.0f, 1.0f, 1.0f);
    m_Scene.MainDirectionalLight.Intensity = 1.0f;

    // Floor
    {
        RenderItem floor;
        floor.Mesh = m_PlaneMesh;
        floor.Material = m_DefaultMaterial;
        floor.Transform.Position = {0.0f, 0.0f, 0.0f};
        floor.Transform.Scale = {10.0f, 1.0f, 10.0f};
        m_Scene.RenderItems.push_back(floor);
    }

    // Cube 1
    {
        RenderItem cube;
        cube.Mesh = m_CubeMesh;
        cube.Material = m_DefaultMaterial;
        cube.Transform.Position = {0.0f, 1.0f, 0.0f};
        cube.Transform.Scale = {1.0f, 1.0f, 1.0f};
        m_Scene.RenderItems.push_back(cube);
    }

    // Cube 2
    {
        RenderItem cube;
        cube.Mesh = m_CubeMesh;
        cube.Material = m_DefaultMaterial;
        cube.Transform.Position = {2.0f, 0.5f, 1.5f};
        cube.Transform.Scale = {1.0f, 1.0f, 1.0f};
        cube.Transform.RotationEulerDegrees = {0.0f, 35.0f, 0.0f};
        m_Scene.RenderItems.push_back(cube);
    }

    // Cube 3
    {
        RenderItem cube;
        cube.Mesh = m_CubeMesh;
        cube.Material = m_DefaultMaterial;
        cube.Transform.Position = {-1.5f, 0.75f, -1.5f};
        cube.Transform.Scale = {1.0f, 1.5f, 1.0f};
        cube.Transform.RotationEulerDegrees = {0.0f, -20.0f, 0.0f};
        m_Scene.RenderItems.push_back(cube);
    }
}

void ShadowMapping::HandleCameraInput(double dt)
{
    if (Input::IsKeyPressed(Key::W))
        m_CameraController.MoveForward(dt);
    if (Input::IsKeyPressed(Key::S))
        m_CameraController.MoveBackward(dt);
    if (Input::IsKeyPressed(Key::A))
        m_CameraController.MoveLeft(dt);
    if (Input::IsKeyPressed(Key::D))
        m_CameraController.MoveRight(dt);
    if (Input::IsKeyPressed(Key::Q))
        m_CameraController.MoveDown(dt);
    if (Input::IsKeyPressed(Key::E))
        m_CameraController.MoveUp(dt);

    if (m_RightMouseLooking)
    {
        std::pair<float, float> mouseDeltaPair = Input::GetMouseDelta();
        glm::vec2 mouseDelta(mouseDeltaPair.first, mouseDeltaPair.second);
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f)
            m_CameraController.OnMouseDelta(mouseDelta.x, -mouseDelta.y);
    }
}