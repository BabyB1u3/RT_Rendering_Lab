# Next Steps — Project Status & Development Plan

Updated 2026-03-14. Single source of truth for project state, priorities, and architecture direction.

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
- Logger macros: null-safe before `Logger::Init()`
- CMake source list: case-correct for `Framebuffer.*`

### Still incomplete or empty

| File                                     | Status     |
|------------------------------------------|------------|
| `src/core/FileSystem.h` / `.cpp`         | Empty stub |
| `src/gui/ImGuiLayer.h` / `.cpp`          | Empty stub |
| `src/gui/Panels/DebugPanel.h` / `.cpp`   | Empty stub |
| `src/gui/Panels/DemoSelectorPanel.h` / `.cpp` | Empty stub |

Other gaps:

- `LabLayer` has a `TODO` for demo selection
- `ShadowMapping::OnImGuiRender()` is empty
- Asset paths are hardcoded relative strings
- No test coverage for `Material`, `SceneRenderer`, or individual passes

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

---

## 3. Development Plan

### Phase 0 — Keep the Base Green ✅

1. ✅ Maintain `ctest` green on the current Windows preset
2. ✅ Update README / roadmap wording so docs do not claim finished GUI or FileSystem
3. ✅ `ActiveCamera` removed from `SceneData`; camera is now passed as `const Camera&` to `SceneRenderer::Render()` — neither `reference_wrapper` nor `Ref<Camera>` needed

Exit criteria: tests green, docs accurate, camera contract decided.

### Phase 1 — Complete the Missing Framework Pieces

Recommended order:

| Order | Item | Rationale |
|-------|------|-----------|
| 1 | Minimal `ImGuiLayer` | Highest dev-experience impact; all panels depend on it |
| 2 | `DemoSelectorPanel` | Resolves the `LabLayer` TODO; makes multi-demo usable |
| 3 | `DebugPanel` | Stats overlay + `SceneRendererSpecification` sliders |
| 4 | `ShadowMapping::OnImGuiRender()` | Expose light direction, shadow tuning, output mode |
| 5 | `FileSystem` | Centralize path resolution; remove hardcoded asset strings |
| 6 | Route shader/asset loading through `FileSystem` | Prerequisite for SPIR-V pipeline later |
| 7 | `KeyCode` enum | Thin wrapper over GLFW constants; stop using raw ints in demos |

Why ImGuiLayer before FileSystem: ImGui gives immediate ability to inspect and tune the renderer at runtime. FileSystem is important for cross-platform correctness but the current `POST_BUILD copy` workflow is sufficient for single-platform dev.

### Phase 2 — Expand the Current Demo & Add Features

1. Allow `SceneRendererSpecification` adjustment at runtime via UI
2. Add a second demo once the selector UI exists

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

### ImGuiLayer

```cpp
class ImGuiLayer : public Layer {
    void OnAttach() override;  // ImGui::CreateContext(), style, backend init
    void OnDetach() override;  // ImGui::DestroyContext()
    void Begin();              // ImGui::NewFrame()
    void End();                // ImGui::Render() + draw data submission
};
```

### DemoSelectorPanel

```cpp
class DemoSelectorPanel {
    void OnImGuiRender(const std::vector<std::string> &demoNames, int &selectedIndex);
};
```

### FileSystem

```cpp
class FileSystem {
    static std::string GetAssetPath(const std::string &relativePath);
    static std::string GetRootPath();
    static std::string ReadTextFile(const std::string &path);
    static bool Exists(const std::string &path);
};
```

### KeyCode (when needed)

```cpp
enum class Key : int {
    W = GLFW_KEY_W, A = GLFW_KEY_A, S = GLFW_KEY_S, D = GLFW_KEY_D,
    Escape = GLFW_KEY_ESCAPE, F1 = GLFW_KEY_F1,
    // extend as demos require
};
```

### Event System (when ImGuiLayer needs input routing)

```cpp
enum class EventType { WindowResize, KeyPressed, KeyReleased, MouseMoved };
struct Event { EventType Type; bool Handled = false; };
struct KeyEvent : Event { Key Code; bool Repeat; };
```

Decision: build `KeyCode.h` first (trivial), build `Event.h` only when `ImGuiLayer` requires input routing.

---

## 5. Items NOT to Focus on Now

- Metal / Vulkan backends
- SPIR-V shader pipeline
- Render graph
- Full multi-backend abstraction sweep
- Backend-aware resource cache

These are valid long-term targets but the current OpenGL path still has missing framework pieces that deliver more immediate value.
