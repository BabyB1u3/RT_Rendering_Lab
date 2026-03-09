# Phase 1 Tasks

## 1. Migrate core rendering wrappers from Illusion
- Copy `RenderCommand` into `src/graphics/`.
- Copy `Buffers` into `src/graphics/`.
- Copy `VertexArray` into `src/graphics/`.
- Copy `Shader` into `src/graphics/`.
- Copy `Texture` into `src/graphics/`.
- Copy `Window` into `src/Core/`.
- Fix include paths and namespaces after the move.
- Make sure these files compile in the new project without depending on old engine modules.

## 2. Expand `RenderCommand`
- Add `SetClearColor`.
- Add a configurable `Clear` function that can clear color, depth, and stencil buffers.
- Add `EnableDepthTest` and `DisableDepthTest`.
- Add `EnableBlending` and `DisableBlending`.
- Add `EnableCullFace` and `DisableCullFace`.
- Add `DrawArrays`.
- Keep `DrawIndexed`.
- Keep `SetViewport`.
- Verify that all OpenGL state changes used in Phase 1 go through `RenderCommand`.

## 3. Clean up buffer and vertex array code
- Review `Buffers` and `VertexArray` after migration.
- Fix integer vertex attribute setup so integer attributes use the correct OpenGL function.
- Check matrix attribute handling for future instancing support.
- Verify vertex layout stride and offset calculations.
- Confirm that VAO/VBO/IBO binding and unbinding behave correctly.

## 4. Upgrade shader handling
- Keep the existing shader compilation and linking flow.
- Add uniform location caching.
- Add or standardize helper functions for:
  - `SetInt`
  - `SetFloat`
  - `SetVec3`
  - `SetVec4`
  - `SetMat4`
- Improve shader compile error output.
- Improve shader link error output.
- Add a placeholder `ReloadFromFile()` interface even if it is not fully used yet.

## 5. Upgrade texture handling
- Keep standard 2D texture creation and binding.
- Add support for creating depth textures for shadow mapping.
- Verify internal format / format / type combinations for:
  - color textures
  - depth textures
- Make sure texture binding by slot works correctly.
- Verify texture parameter setup for filtering and wrapping.

## 6. Implement `Framebuffer`
- Create `src/graphics/Framebuffer.h/.cpp`.
- Implement framebuffer creation and destruction.
- Support at least:
  - depth-only framebuffer
  - color + depth-stencil framebuffer
- Add attachment creation logic for color attachments.
- Add attachment creation logic for depth attachments.
- Add `Bind()` and `Unbind()`.
- Add `Resize(width, height)`.
- Add accessors for color and depth attachments.
- Validate framebuffer completeness after setup.
- Test framebuffer resizing with window resize.

## 7. Implement `Mesh`
- Create `src/graphics/Mesh.h/.cpp`.
- Define a mesh class that owns or references:
  - vertex array
  - vertex buffer
  - index buffer
  - index count
- Add `Bind()`.
- Add `GetIndexCount()`.
- Make sure a mesh can be rendered through `RenderCommand::DrawIndexed`.

## 8. Implement `Material`
- Create `src/graphics/Material.h/.cpp`.
- Store a shader reference.
- Store bound textures by slot.
- Add `SetShader(...)`.
- Add `SetTexture(slot, texture)`.
- Add `Bind()` to bind shader and textures.
- Keep the first version minimal; do not add a large parameter system yet.

## 9. Implement `MeshFactory`
- Create `src/graphics/MeshFactory.h/.cpp`.
- Add `CreateCube()`.
- Add `CreatePlane()`.
- Add `CreateFullscreenQuad()`.
- Move vertex/index data for these primitives into `MeshFactory`.
- Remove any dependency on old `Renderer(Type)`-style construction.

## 10. Implement basic camera support
- Create or adapt `src/Scene/Camera.h/.cpp`.
- Support perspective projection.
- Support orthographic projection if needed by existing code.
- Add view matrix calculation.
- Add projection matrix calculation.
- Add combined view-projection access.

## 11. Implement `DebugCameraController`
- Create `src/Scene/DebugCameraController.h/.cpp`.
- Reuse the old FPS-style camera interaction ideas.
- Support mouse look.
- Support WASD movement.
- Support vertical movement if desired.
- Support scroll-wheel zoom if needed.
- Keep it independent from any `Player` or gameplay class.

## 12. Define minimal scene data structures
- Create `src/Scene/Transform.h`.
- Create `src/Scene/Light.h`.
- Create `src/Scene/SceneData.h`.
- Add a transform structure or helper for model matrices.
- Add a simple directional light structure.
- Add a `SceneData` structure that stores:
  - active camera
  - light data
  - render items list or references

## 13. Define render submission data
- Create `src/Renderer/RenderItem.h`.
- Define a render item containing:
  - mesh reference
  - material reference
  - transform
- Keep it minimal and explicit.

## 14. Implement pass base interface
- Create `src/Renderer/Passes/RenderPass.h`.
- Define a small base interface for render passes.
- Include methods for initialization, execution, and resize handling.

## 15. Implement `ShadowPass`
- Create `src/Renderer/Passes/ShadowPass.h/.cpp`.
- Create and manage a depth-only framebuffer for the shadow map.
- Store shadow map resolution.
- Compute light view-projection matrix for the directional light.
- Render all submitted meshes into the depth framebuffer.
- Output the depth texture for later passes.

## 16. Implement `ForwardPass`
- Create `src/Renderer/Passes/ForwardPass.h/.cpp`.
- Create and manage the main render target if needed.
- Render submitted meshes using the active camera.
- Bind light data.
- Bind the shadow map generated by `ShadowPass`.
- Render plane and cubes with a basic shadowed forward shader.

## 17. Implement `TexturePreviewPass`
- Create `src/Renderer/Passes/TexturePreviewPass.h/.cpp`.
- Use a fullscreen quad from `MeshFactory`.
- Render an arbitrary texture to the screen.
- Support previewing the shadow map texture.

## 18. Implement `SceneRenderer`
- Create `src/Renderer/SceneRenderer.h/.cpp`.
- Add `BeginScene(...)`.
- Add `Submit(...)`.
- Add `EndScene()`.
- Store submitted `RenderItem`s for the current frame.
- Connect submitted items to `ShadowPass` and `ForwardPass`.
- Keep the first version simple; no sorting or batching is required yet.

## 19. Create experiment module `ShadowMappingLab`
- Create `src/Experiments/ShadowMappingLab.h/.cpp`.
- Initialize:
  - one plane mesh
  - several cube meshes or instances
  - one directional light
  - one debug camera
  - one shadow pass
  - one forward pass
  - one texture preview pass
- Build a simple scene manually in code.
- Add a toggle to switch between:
  - normal scene rendering
  - shadow map preview

## 20. Add shaders required for Phase 1
- Create a depth-only shader for shadow map generation.
- Create a forward lighting shader with shadow sampling.
- Create a fullscreen texture preview shader.
- Place shader files in the new project’s shader directory.
- Update file paths used by the new project.

## 21. Integrate window resize handling
- Detect framebuffer/window resize events.
- Resize the main framebuffer when needed.
- Resize any preview targets if needed.
- Update viewport through `RenderCommand`.
- Update camera projection aspect ratio after resize.

## 22. Test the full Phase 1 pipeline
- Verify window creation and OpenGL initialization.
- Verify camera movement and mouse look.
- Verify cube rendering.
- Verify plane rendering.
- Verify shadow map generation.
- Verify shadow map sampling in the forward pass.
- Verify fullscreen texture preview.
- Verify resize behavior.
- Verify that all rendering works without depending on old game-specific classes.

## 23. Remove or avoid old system patterns
- Do not introduce global renderer state objects like the old `Game.cpp`.
- Do not reintroduce `Renderer(Type)`-style hardcoded primitive renderers.
- Do not couple render passes to gameplay classes.
- Do not couple `graphics` to scene or gameplay logic.
- Do not use the old string-based global resource manager as a core dependency.

## 24. Final cleanup for Phase 1
- Check naming consistency across all new files.
- Remove dead code from migrated files.
- Remove unused includes.
- Verify that each module only depends on the correct layer.
- Confirm that a new rendering experiment can be added without modifying `graphics`.