#pragma once

/// @file RenderPass.h
/// @brief Abstract base class for all render passes in the multi-pass pipeline.
///
/// Each pass implements Execute(const RenderContext&) to perform its draw work.
/// The RenderContext provides everything a pass needs: scene view, shared frame
/// resources (shadow map, scene color), renderer specification, and output mode.
///
/// Current pipeline order (driven by SceneRenderer::Render):
///   ShadowPass → ForwardPass → TexturePreviewPass

struct RenderContext;

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    /// Called when the viewport or framebuffer size changes.
    virtual void Resize(unsigned int width, unsigned int height) = 0;
    /// Execute this pass's rendering work using the provided frame context.
    virtual void Execute(const RenderContext& ctx) = 0;
};