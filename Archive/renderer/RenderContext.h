#pragma once

// RenderContext.h - Per-frame rendering state passed to all render passes.
//
// Design rationale:
//   SceneView    - (which camera looks at which scene)
//                  Decouples scene content from rendering viewpoint. To support
//                  editor camera, game camera, or preview camera in the future,
//                  simply construct a different SceneView.
//
//   FrameResources - (inter-pass shared outputs)
//                    Each pass still owns its framebuffer and render target
//                    internally. FrameResources holds Ref<ITexture2D> pointing
//                    to attachment textures so that downstream passes can sample
//                    upstream outputs.
//                    Extension: add output textures here when new passes
//                    are introduced.
//
//   RenderContext  - (everything a pass needs)
//                    The unified parameter for Execute(). Passes read inputs and
//                    write outputs through this struct.
//                    Extension: add debug flags, render stats, feature toggles, etc.
//
// Ownership convention:
//   const T&  = borrow (non-owning). Caller guarantees the referenced object
//               outlives the reference. All references here are scoped to the
//               lifetime of SceneRenderer::Render().
//   Ref<T>    = shared ownership (shared_ptr). Used for GPU resources (textures,
//               framebuffers) where multiple sites need to hold the same handle.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "renderer/SceneRendererTypes.h"

class Camera;
class IRenderTarget;
class ITexture2D;
struct SceneData;

struct SceneView
{
    const SceneData &Scene;
    const Camera &Camera;
    uint32_t ViewportWidth = 0;
    uint32_t ViewportHeight = 0;
};

struct FrameResources
{
    glm::mat4 LightViewProjection{1.0f};

    /// Shadow depth texture produced by ShadowPass.
    Ref<ITexture2D> ShadowMap;

    /// Scene color texture produced by ForwardPass.
    Ref<ITexture2D> SceneColor;

    /// Back buffer render target for final output (owned by SceneRenderer).
    Ref<IRenderTarget> BackBuffer;
};

struct RenderContext
{
    const SceneView &View;
    const SceneRendererSpecification &Spec;
    FrameResources Resources;
    SceneRendererOutput OutputMode = SceneRendererOutput::FinalColor;
};
