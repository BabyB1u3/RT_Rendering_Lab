#include "MaterialPlayground.h"

#include <glm/glm.hpp>

#include <imgui.h>

#include "core/Input.h"
#include "core/KeyCode.h"
#include "core/Logger.h"
#include "core/MouseCode.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/Material.h"
#include "graphics/MeshFactory.h"
#include "graphics/Texture.h"
#include "graphics/interface/ITexture2D.h"
#include "renderer/RenderItem.h"

MaterialPlayground::MaterialPlayground(uint32_t width, uint32_t height)
    : m_ViewportWidth(width),
      m_ViewportHeight(height),
      m_Camera(45.0f, static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f),
      m_CameraController(&m_Camera)
{
}

void MaterialPlayground::OnAttach()
{
    LOG_INFO("MaterialPlayground demo attached");
    m_Renderer = CreateScope<SceneRenderer>(m_ViewportWidth, m_ViewportHeight);

    m_Camera.SetPosition({0.0f, 3.0f, 10.0f});
    m_Camera.SetRotation(-90.0f, -15.0f);

    m_CameraController.SetMoveSpeed(5.0f);
    m_CameraController.SetMouseSensitivity(0.12f);

    m_SphereMesh = MeshFactory::CreateSphere();
    m_PlaneMesh = MeshFactory::CreatePlane();

    TextureSpecification whiteSpec;
    whiteSpec.Width = 1;
    whiteSpec.Height = 1;
    whiteSpec.Format = TextureFormat::RGBA8;
    m_WhiteTexture = GetDevice()->CreateTexture2D(whiteSpec);
    const uint32_t whitePixel = 0xFFFFFFFFu;
    m_WhiteTexture->SetData(&whitePixel);

    m_FloorMaterial = CreateRef<Material>();
    m_FloorMaterial->SetTexture(TextureSlot::Albedo, m_WhiteTexture);
    m_FloorMaterial->SetVec3("u_Albedo", glm::vec3(0.6f));
    m_FloorMaterial->SetFloat("u_SpecularPower", 16.0f);
    m_FloorMaterial->SetFloat("u_AmbientStrength", 0.15f);

    m_Spheres = {
        {"Matte Red", {0.9f, 0.2f, 0.15f}, 4.0f, 0.2f, nullptr},
        {"Shiny Blue", {0.2f, 0.3f, 0.9f}, 128.0f, 0.1f, nullptr},
        {"Gold", {0.83f, 0.69f, 0.22f}, 64.0f, 0.12f, nullptr},
        {"Plastic Green", {0.1f, 0.8f, 0.2f}, 32.0f, 0.15f, nullptr},
        {"Chrome", {0.9f, 0.9f, 0.9f}, 256.0f, 0.05f, nullptr},
    };

    for (auto &s : m_Spheres)
    {
        s.Mat = CreateRef<Material>();
        s.Mat->SetTexture(TextureSlot::Albedo, m_WhiteTexture);
        s.Mat->SetVec3("u_Albedo", s.Albedo);
        s.Mat->SetFloat("u_SpecularPower", s.SpecularPower);
        s.Mat->SetFloat("u_AmbientStrength", s.AmbientStrength);
    }

    BuildScene();
}

void MaterialPlayground::OnDetach()
{
    LOG_INFO("MaterialPlayground demo detached");
    m_Scene.RenderItems.clear();
    m_Spheres.clear();
    m_FloorMaterial.reset();
    m_WhiteTexture.reset();
    m_SphereMesh.reset();
    m_PlaneMesh.reset();
    m_Renderer.reset();
}

void MaterialPlayground::OnUpdate(double dt)
{
    HandleCameraInput(dt);

    if (Input::WasKeyPressedThisFrame(Key::D1))
        m_OutputMode = SceneRendererOutput::FinalColor;
    if (Input::WasKeyPressedThisFrame(Key::D2))
        m_OutputMode = SceneRendererOutput::ShadowMap;
}

void MaterialPlayground::OnRender()
{
    if (!m_Renderer)
        return;

    m_Renderer->SetOutputMode(m_OutputMode);
    m_Renderer->Render(m_Scene, m_Camera);
}

void MaterialPlayground::OnImGuiRender()
{
    ImGui::Begin("Material Playground");

    int mode = static_cast<int>(m_OutputMode);
    ImGui::Text("Output Mode");
    ImGui::RadioButton("Final Color", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Shadow Map", &mode, 1);
    m_OutputMode = static_cast<SceneRendererOutput>(mode);

    ImGui::Separator();

    ImGui::Text("Directional Light");
    ImGui::DragFloat3("Direction", &m_Scene.MainDirectionalLight.Direction.x, 0.01f, -1.0f, 1.0f);
    m_Scene.MainDirectionalLight.Direction = glm::normalize(m_Scene.MainDirectionalLight.Direction);
    ImGui::ColorEdit3("Light Color", &m_Scene.MainDirectionalLight.Color.x);
    ImGui::DragFloat("Intensity", &m_Scene.MainDirectionalLight.Intensity, 0.01f, 0.0f, 5.0f);

    ImGui::Separator();

    for (size_t i = 0; i < m_Spheres.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::CollapsingHeader(m_Spheres[i].Label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::ColorEdit3("Albedo", &m_Spheres[i].Albedo.x);
            ImGui::DragFloat("Specular Power", &m_Spheres[i].SpecularPower, 1.0f, 1.0f, 512.0f);
            ImGui::DragFloat("Ambient", &m_Spheres[i].AmbientStrength, 0.005f, 0.0f, 1.0f);
        }
        ImGui::PopID();
    }

    SyncMaterialProperties();

    ImGui::End();
}

void MaterialPlayground::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_ViewportWidth = width;
    m_ViewportHeight = height;
    m_Camera.SetViewportSize(width, height);

    if (m_Renderer)
        m_Renderer->Resize(width, height);
}

void MaterialPlayground::BuildScene()
{
    m_Scene.RenderItems.clear();

    m_Scene.MainDirectionalLight.Direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    m_Scene.MainDirectionalLight.Color = glm::vec3(1.0f);
    m_Scene.MainDirectionalLight.Intensity = 1.2f;

    // Floor
    {
        RenderItem floor;
        floor.Mesh = m_PlaneMesh;
        floor.Material = m_FloorMaterial;
        floor.Transform.Position = {0.0f, 0.0f, 0.0f};
        floor.Transform.Scale = {15.0f, 1.0f, 15.0f};
        m_Scene.RenderItems.push_back(floor);
    }

    // Spheres — evenly spaced along X axis, centered at origin.
    for (size_t i = 0; i < m_Spheres.size(); ++i)
    {
        RenderItem sphere;
        sphere.Mesh = m_SphereMesh;
        sphere.Material = m_Spheres[i].Mat;
        sphere.Transform.Position = {(static_cast<float>(i) - 2.0f) * 2.5f, 1.0f, 0.0f};
        sphere.Transform.Scale = {1.5f, 1.5f, 1.5f};
        m_Scene.RenderItems.push_back(sphere);
    }
}

/// Push UI-edited SpherePreset values into the actual Material objects.
/// Called every frame after ImGui editing so that the renderer sees updated values.
void MaterialPlayground::SyncMaterialProperties()
{
    for (auto &s : m_Spheres)
    {
        s.Mat->SetVec3("u_Albedo", s.Albedo);
        s.Mat->SetFloat("u_SpecularPower", s.SpecularPower);
        s.Mat->SetFloat("u_AmbientStrength", s.AmbientStrength);
    }
}

void MaterialPlayground::HandleCameraInput(double dt)
{
    if (Input::IsKeyDown(Key::W))
        m_CameraController.MoveForward(dt);
    if (Input::IsKeyDown(Key::S))
        m_CameraController.MoveBackward(dt);
    if (Input::IsKeyDown(Key::A))
        m_CameraController.MoveLeft(dt);
    if (Input::IsKeyDown(Key::D))
        m_CameraController.MoveRight(dt);
    if (Input::IsKeyDown(Key::Q))
        m_CameraController.MoveDown(dt);
    if (Input::IsKeyDown(Key::E))
        m_CameraController.MoveUp(dt);

    // Right-click to look around
    if (Input::IsMouseButtonDown(Mouse::Right))
    {
        auto [dx, dy] = Input::GetMouseDelta();
        if (dx != 0.0f || dy != 0.0f)
            m_CameraController.OnMouseDelta(dx, -dy);
    }

    // Scroll to zoom (adjust FOV)
    float scroll = Input::GetScrollDelta();
    if (scroll != 0.0f)
        m_CameraController.OnMouseScroll(scroll);
}
