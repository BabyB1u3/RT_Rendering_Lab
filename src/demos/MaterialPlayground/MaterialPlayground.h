#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "demos/DemoBase.h"
#include "renderer/SceneRenderer.h"
#include "scene/Camera.h"
#include "scene/DebugCameraController.h"
#include "scene/SceneData.h"

class Mesh;
class Material;
class Texture2D;

class MaterialPlayground : public DemoBase
{
public:
    MaterialPlayground(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void BuildScene();
    void HandleCameraInput(double dt);
    void SyncMaterialProperties();

private:
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;

    Camera m_Camera;
    DebugCameraController m_CameraController;

    Scope<SceneRenderer> m_Renderer;
    SceneData m_Scene;

    Ref<Mesh> m_SphereMesh;
    Ref<Mesh> m_PlaneMesh;

    Ref<Texture2D> m_WhiteTexture;
    Ref<Material> m_FloorMaterial;

    struct SpherePreset
    {
        std::string Label;
        glm::vec3 Albedo;
        float SpecularPower;
        float AmbientStrength;
        Ref<Material> Mat;
    };

    std::vector<SpherePreset> m_Spheres;

    SceneRendererOutput m_OutputMode = SceneRendererOutput::FinalColor;
};
