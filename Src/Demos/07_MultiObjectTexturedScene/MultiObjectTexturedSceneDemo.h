#pragma once

/// @file MultiObjectTexturedSceneDemo.h
/// @brief Tutorial 07: draw a small multi-object textured scene.

#include <array>
#include <cstdint>

#include "Core/Util/Base.h"
#include "Demos/DemoBase.h"
#include "Render/RHI/RHI.h"
#include "Render/Renderer/ForwardRenderer.h"

class MultiObjectTexturedSceneDemo : public DemoBase
{
public:
    MultiObjectTexturedSceneDemo(uint32_t width, uint32_t height);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    static constexpr uint32_t kMaterialCount = 2;
    static constexpr uint32_t kObjectCount = 3;

    void CreateSceneResources();
    void CreateDepthResources();
    void UploadPendingResources(CommandList& commandList, ResourceStateTracker& resourceStateTracker);
    void UpdateSceneParameters();
    void ForgetTrackedResources();

    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
    float m_ElapsedSeconds = 0.0f;

    Scope<Buffer> m_VertexUploadBuffer;
    Scope<Buffer> m_VertexBuffer;
    Scope<Buffer> m_IndexUploadBuffer;
    Scope<Buffer> m_IndexBuffer;
    std::array<Scope<Buffer>, kMaterialCount> m_TextureUploadBuffers;
    bool m_GeometryUploadPending = false;
    std::array<bool, kMaterialCount> m_TextureUploadPending = {};

    std::array<Scope<Texture>, kMaterialCount> m_Textures;
    std::array<Scope<TextureView>, kMaterialCount> m_TextureViews;
    Scope<Sampler> m_Sampler;
    Scope<Texture> m_DepthTexture;
    Scope<TextureView> m_DepthView;

    Renderer::ForwardRenderer m_Renderer;
    Renderer::Mesh m_Mesh;
    std::array<Renderer::Material, kMaterialCount> m_Materials;
    std::array<Renderer::RenderObject, kObjectCount> m_RenderObjects;
    std::array<Scope<ResourceSet>, kMaterialCount> m_MaterialSets;
    std::array<Scope<ResourceSet>, kObjectCount> m_ObjectSets;
};
