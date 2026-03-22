# Next Steps — Project Status & Development Plan

Updated 2026-03-22. Single source of truth for project state, priorities, and architecture direction.

> **Architecture Goal**: Multi-backend support (OpenGL on Windows/Linux, Metal on macOS, Vulkan optional).
> All priorities below reflect this direction, but immediate work focuses on completing the current
> OpenGL foundation before starting the backend refactor.

---

## 1. Current Snapshot

| Metric               | Status                                             |
|----------------------|----------------------------------------------------|
| Phase 0 (Base Green) | Complete                                           |
| Phase 1 (Framework)  | Complete                                           |
| Phase 2 (Basic RT)   | ~60% — lighting, shadows, Blinn-Phong done         |
| Phase 3 (Pass Unify) | Complete                                           |
| Active Demos         | 1 (Shadow Mapping)                                 |
| Shader Files         | 3 (ForwardLit, ShadowDepth, TexturePreview)        |
| Test Suite           | 77 / 77 passing (unit + integration, Google Test)  |
| Rendering Pipeline   | Forward: ShadowPass -> ForwardPass -> TexturePreview |
| Platform Support     | Windows only (OpenGL 4.6)                          |

### Working today

- Core runtime: `Application`, `Window`, `Input`, `Layer`, `LayerStack`, `Time`, `Logger`
- OpenGL resources: buffers, vertex arrays, textures, shaders, framebuffers, meshes
- Scene layer: `Camera`, `DebugCameraController`, `Transform`, `DirectionalLight`, `SceneData`
- Renderer: `SceneRenderer`, `RenderContext` (SceneView + FrameResources), `ShadowPass`, `ForwardPass`, `TexturePreviewPass`
- Demo system: `DemoBase`, `DemoRegistry`, `LabLayer`, `ShadowMapping`
- Material: textures + typed float/int/vec3/vec4 properties with `UploadToShader()`
- `RenderTarget`: backbuffer vs framebuffer wrapper, depth-only safe, integration-tested
- `SceneRendererSpecification` / `SceneRendererOutput`: renderer tuning extracted into `SceneRendererTypes.h`
- `FileSystem`: cross-platform path resolution, `GetAssetPath()`, `GetShaderPath()`, `ReadTextFile()`, integrated into Application and all render passes
- `Input`: polling-based keyboard/mouse via GLFW (`IsKeyPressed()`, `GetMousePosition()`, `GetMouseDelta()`)
- `ImGuiLayer`: GLFW+OpenGL3 backend, auto-created as overlay in `Application`, `Begin()`/`End()` wrapping all `OnImGuiRender()` calls
- `DemoSelectorPanel`: selectable list from `DemoRegistry::GetNames()`, integrated into `LabLayer` with runtime demo switching
- `DebugPanel`: FPS / frame time display, owned by `LabLayer` (global scope, not per-demo)
- `ShadowMapping::OnImGuiRender()`: output mode toggle, light direction/color/intensity, light projection tuning via `SceneRendererSpecification`
- Logger macros: null-safe before `Logger::Init()`
- CMake source list: case-correct for `Framebuffer.*`

### Remaining gaps

- Input/Event system Layer 1-2 (double-buffered state, InputAction map) not yet implemented
- No test coverage for `Material`, `SceneRenderer`, or individual passes
- Only one demo registered; demo selector UI is ready for more

---

## 2. Recently Resolved

These were identified and fixed in the current sprint:

- Logger macros no longer crash when called before `Logger::Init()`
- `RenderTarget::GetColorAttachment()` returns `nullptr` on depth-only framebuffers
- CMake source list uses correct `Framebuffer` casing for cross-platform builds
- All 7 previously failing tests now pass (LayerStack, Shader, RenderTarget)
- `ActiveCamera` removed from `SceneData`; camera passed via `Render(scene, camera)` parameter
- `RenderPass` now has unified `Execute(const RenderContext&)` pure virtual interface
- Introduced `SceneView` / `FrameResources` / `RenderContext` three-layer frame state design
- Extracted `SceneRendererSpecification` / `SceneRendererOutput` into `SceneRendererTypes.h`
- `FileSystem` fully implemented with cross-platform path discovery and integrated into Application, SceneRenderer, and all render passes
- `Input` polling system implemented (`IsKeyPressed`, `GetMousePosition`, `GetMouseDelta`)
- Input/Event system architecture designed (10-layer spec in `docs/design-input-event-system.md`)
- Archived old Illusion panels to `archive/Illusion/`
- `ImGuiLayer` implemented (GLFW+OpenGL3 backend), auto-created as overlay in `Application`
- `DemoSelectorPanel` implemented with runtime demo switching in `LabLayer`
- `DebugPanel` implemented (FPS/frame time), owned by `LabLayer` as global panel
- `ShadowMapping::OnImGuiRender()` implemented: output mode, light params, light projection tuning
- `SceneRenderer::GetSpecification()` added for runtime spec access
- `KeyCode.h` / `MouseCode.h` implemented per design doc Layer 0 (`Key::Code`, `Mouse::Code` typed enums)
- `ShadowMapping` migrated from hardcoded `int` constants to `Key::` / `Mouse::` enums

---

## 3. Development Plan

### Phase 0 — Keep the Base Green ✅

1. ✅ Maintain `ctest` green on the current Windows preset
2. ✅ Update README / roadmap wording so docs do not claim finished GUI or FileSystem
3. ✅ `ActiveCamera` removed from `SceneData`; camera is now passed as `const Camera&` to `SceneRenderer::Render()` — neither `reference_wrapper` nor `Ref<Camera>` needed

Exit criteria: tests green, docs accurate, camera contract decided.

### Phase 1 — Complete the Missing Framework Pieces

Recommended order:

| Order | Item | Status | Rationale |
|-------|------|--------|-----------|
| 1 | Minimal `ImGuiLayer` | ✅ Done | Highest dev-experience impact; all panels depend on it |
| 2 | `DemoSelectorPanel` | ✅ Done | Resolves the `LabLayer` TODO; makes multi-demo usable |
| 3 | `DebugPanel` | ✅ Done | FPS/frame time display (global, owned by LabLayer) |
| 4 | `ShadowMapping::OnImGuiRender()` | ✅ Done | Output mode, light direction, light projection tuning |
| 5 | `FileSystem` | ✅ Done | Centralize path resolution; remove hardcoded asset strings |
| 6 | Route shader/asset loading through `FileSystem` | ✅ Done | Prerequisite for SPIR-V pipeline later |
| 7 | `KeyCode` / `MouseCode` | ✅ Done | Typed enums matching design doc Layer 0; `ShadowMapping` migrated |
| 8 | Input/Event system (Layer 1-2) | **TODO** | Double-buffered state + InputAction map; design doc complete |

Items 1-7 completed. Phase 1 only remaining item is Layer 1-2 of Input/Event system. Next priority is Phase 2 (add a second demo).

### Phase 2 — Expand the Current Demo & Add Features

1. ✅ Allow `SceneRendererSpecification` adjustment at runtime via UI (done in `ShadowMapping::OnImGuiRender()`)
2. Add a second demo (selector UI is ready)

Recommended second demo candidates (pick one):

- Material Playground — exercise the property system
- Normal Mapping — high visual impact, small shader change
- Skybox / Environment Preview — simple, visually striking

Then continue with:

3. Model loading (OBJ or glTF via Assimp/tinygltf)
4. Normal mapping (TBN in vertex shader, modify `ForwardLit.glsl`)
5. Skybox / environment map

Rationale: model loading unlocks real test assets; normal mapping and skybox are both more useful once real geometry exists.

### Phase 3 — Unify Pass Interface (Pre-Refactor Architecture) ✅

Adopted a three-layer design instead of the original string-keyed map approach:

```cpp
// SceneView: decouples scene content from rendering viewpoint
struct SceneView {
    const SceneData& Scene;   // borrowed from demo
    const Camera& Camera;     // borrowed from demo
    uint32_t ViewportWidth, ViewportHeight;
};

// FrameResources: inter-pass shared outputs (typed fields, not string maps)
struct FrameResources {
    glm::mat4 LightViewProjection{1.0f};
    Ref<Texture2D> ShadowMap;
    RenderTarget ShadowTarget;
    Ref<Texture2D> SceneColor;
    RenderTarget SceneTarget;
};

// RenderContext: unified parameter for Execute()
struct RenderContext {
    const SceneView& View;
    const SceneRendererSpecification& Spec;
    FrameResources Resources;
    SceneRendererOutput OutputMode = SceneRendererOutput::FinalColor;
};
```

1. ✅ Introduced `SceneView`, `FrameResources`, `RenderContext` in `src/renderer/RenderContext.h`
2. ✅ All passes implement `Execute(const RenderContext&)` via pure virtual on `RenderPass`
3. ✅ `SceneRenderer::Render()` now just builds context and calls three passes sequentially
4. ✅ Extracted `SceneRendererSpecification` / `SceneRendererOutput` into `SceneRendererTypes.h`

Why typed fields over string maps: compile-time safety, IDE support, no typo risk. Suitable for the current 3-pass pipeline; render graph with dynamic resource names is a Phase 5+ concern.

### Phase 4 — Multi-Backend Refactor

Do not begin until Phases 0-3 are complete.

#### R1: Abstract Interface Layer

Define pure virtual interfaces in `src/graphics/interface/`:

```
IVertexBuffer, IIndexBuffer, IVertexArray, ITexture2D,
IFramebuffer, IRenderTarget, IShader, IUniformBuffer,
IRenderCommand, IGraphicsDevice (factory)
```

#### R2: OpenGL Backend Reorganization

Move existing OpenGL code into `src/graphics/opengl/` implementing the R1 interfaces. Mostly mechanical file moves + adding virtual overrides.

#### R3: Shader Pipeline (SPIR-V)

GLSL -> SPIR-V (via glslang/shaderc, offline CMake step) -> SPIRV-Cross for per-backend transpilation. Single shader source, multiple backends.

#### R4: Metal Backend

Implement via metal-cpp in `src/graphics/metal/*.mm`. CMake selects backend at configure time.

#### R5: Vulkan Backend (Optional)

Consumes SPIR-V directly. Primarily useful if OpenGL is to be phased out on Windows/Linux.

### Phase 5 — Post-Refactor Features

| Order | Feature                    | Depends On         |
|-------|----------------------------|--------------------|
| A     | Multiple Light Sources     | `IUniformBuffer`   |
| B     | HDR + Tone Mapping + Gamma | —                  |
| C     | PBR (Cook-Torrance BRDF)   | HDR, Material      |
| D     | Deferred Rendering         | Render Graph       |
| E     | SSAO                       | Deferred G-Buffer  |
| F     | Bloom                      | HDR pipeline       |

### Post-Refactor Architecture

- Backend-aware resource cache (`ShaderLibrary` / `TextureCache` over `IShader` / `ITexture2D`)
- Render graph (pass DAG with automatic resource lifetime management)
- Forward declarations in headers (when compile times become noticeable)

---

## 4. Placeholder File Reference

### ImGuiLayer ✅ (Implemented)

Now in `src/gui/ImGuiLayer.h/.cpp`. GLFW+OpenGL3 backend, auto-created as overlay in `Application`. `Begin()`/`End()` wrap all `OnImGuiRender()` calls.

### DemoSelectorPanel ✅ (Implemented)

Now in `src/gui/Panels/DemoSelectorPanel.h/.cpp`. Selectable list from `DemoRegistry::GetNames()`, integrated into `LabLayer` with runtime demo switching.

### FileSystem ✅ (Implemented)

Now in `src/core/FileSystem.h/.cpp`. Cross-platform path discovery with `Init()`, `GetRootPath()`, `GetAssetPath()`, `GetShaderPath()`, `ReadTextFile()`, `Exists()`.

### KeyCode / MouseCode ✅ (Implemented)

Now in `src/core/KeyCode.h` and `src/core/MouseCode.h`. Typed enums (`Key::Code`, `Mouse::Code`) matching GLFW values, per design doc Layer 0. No GLFW header dependency outside `Input.cpp` / `Window.cpp`.

### Event System (when needed)

Full Input/Event system architecture specified in `docs/design-input-event-system.md` (10-layer design). Next implementation step is Layer 1 (double-buffered polling state) and Layer 2 (InputAction map).

---

## 5. Items NOT to Focus on Now

- Metal / Vulkan backends
- SPIR-V shader pipeline
- Render graph
- Full multi-backend abstraction sweep
- Backend-aware resource cache

These are valid long-term targets but the current OpenGL path still has missing framework pieces that deliver more immediate value.
