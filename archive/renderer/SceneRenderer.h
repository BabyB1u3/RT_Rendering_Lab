#pragma once

/// @file SceneRenderer.h
/// @brief Multi-pass scene rendering orchestrator.
///
/// Owns and drives the three-pass forward pipeline:
///   1. ShadowPass   - depth map from the directional light's POV
///   2. ForwardPass   - Blinn-Phong shading + shadow sampling → scene color FBO
///   3. TexturePreviewPass - blit selected output to back buffer
///
/// Render() builds a RenderContext (SceneView + FrameResources) and calls
/// Execute() on each pass sequentially. Demos create one SceneRenderer and
/// feed it a SceneData + Camera each frame.
///
/// The specification (shadow resolution, light projection, shaders, clear color)
/// is exposed via GetSpecification() for runtime ImGui tuning.

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "renderer/SceneRendererTypes.h"

class Camera;
class IFramebuffer;
class IRenderTarget;
class ITexture2D;
class ShadowPass;
class ForwardPass;
class TexturePreviewPass;
struct SceneData;
struct DirectionalLight;

class SceneRenderer
{
public:
    SceneRenderer(uint32_t width, uint32_t height, const SceneRendererSpecification &spec = {});

    void Resize(uint32_t width, uint32_t height);

    void SetOutputMode(SceneRendererOutput mode) { m_OutputMode = mode; }
    SceneRendererOutput GetOutputMode() const { return m_OutputMode; }

    SceneRendererSpecification &GetSpecification() { return m_Spec; }
    const SceneRendererSpecification &GetSpecification() const { return m_Spec; }

    /// Run the full multi-pass pipeline for one frame. Camera is passed by const ref
    /// (not stored in SceneData) so that different viewpoints can render the same scene.
    void Render(const SceneData &scene, const Camera &camera);

    Ref<ShadowPass> GetShadowPass() const { return m_ShadowPass; }
    Ref<ForwardPass> GetForwardPass() const { return m_ForwardPass; }

private:
    /// Construct the light-space view-projection matrix for shadow mapping.
    /// Uses a simple orthographic projection centered at the origin; a future
    /// improvement could fit the frustum tightly around the visible scene.
    glm::mat4 BuildDirectionalLightViewProjection(const DirectionalLight &light) const;

private:
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    SceneRendererOutput m_OutputMode = SceneRendererOutput::FinalColor;

    SceneRendererSpecification m_Spec;

    Ref<ShadowPass> m_ShadowPass;
    Ref<ForwardPass> m_ForwardPass;
    Ref<TexturePreviewPass> m_TexturePreviewPass;

    /// Cached back buffer render target (P5). Resized on viewport change.
    Ref<IRenderTarget> m_BackBufferTarget;
};
