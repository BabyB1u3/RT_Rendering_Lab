# Next Steps — Project Status & Development Plan

Updated 2026-03-22. Single source of truth for project state, priorities, and architecture direction.

> **Architecture Goal**: Multi-backend support (OpenGL on Windows/Linux, Metal on macOS, Vulkan optional).
> Phase 4 (Multi-Backend Refactor) is now active — starting with R1 (Abstract Interface Layer).
> Phase 1 (Input Layer 1-2) and Phase 2 (model loading, normal mapping, skybox) remain in progress
> but do not block R1: they operate on the renderer/demo layer while R1 operates on the graphics
> abstraction layer.

---

## 1. Current Snapshot

| Metric               | Status                                             |
|----------------------|----------------------------------------------------|
| Phase 0 (Base Green) | Complete                                           |
| Phase 1 (Framework)  | 7/8 done — Input Layer 1-2 deferred                |
| Phase 2 (Basic RT)   | ~60% — lighting, shadows, Blinn-Phong done         |
| Phase 3 (Pass Unify) | Complete                                           |
| **Phase 4 (Refactor)** | **Active — R1 in progress**                      |
| Active Demos         | 2 (Shadow Mapping, Material Playground)            |
| Shader Files         | 3 (ForwardLit, ShadowDepth, TexturePreview)        |
| Test Suite           | 77 / 77 passing (unit + integration, Google Test)  |
| Rendering Pipeline   | Forward: ShadowPass -> ForwardPass -> TexturePreview |
| Platform Support     | Windows only (OpenGL 4.6)                          |

### Working today

- Core runtime: `Application`, `Window`, `Input`, `Layer`, `LayerStack`, `Time`, `Logger`
- OpenGL resources: buffers, vertex arrays, textures, shaders, framebuffers, meshes
- Scene layer: `Camera`, `DebugCameraController`, `Transform`, `DirectionalLight`, `SceneData`
- Renderer: `SceneRenderer`, `RenderContext` (SceneView + FrameResources), `ShadowPass`, `ForwardPass`, `TexturePreviewPass`
- Demo system: `DemoBase`, `DemoRegistry`, `LabLayer`, `ShadowMapping`, `MaterialPlayground`
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
- `MeshFactory::CreateSphere()`: UV sphere primitive (16 stacks × 32 slices)
- CMake source list: case-correct for `Framebuffer.*`

### Remaining gaps

- Input/Event system Layer 1-2 (double-buffered state, InputAction map) not yet implemented
- No test coverage for `Material`, `SceneRenderer`, or individual passes
- NVIDIA GL performance warning (id=131218): shader recompilation on first draw — cosmetic, not functional

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
- `MeshFactory::CreateSphere()` added — UV sphere with configurable stacks/slices
- `MaterialPlayground` demo added: 5 spheres with distinct Blinn-Phong presets, ImGui per-sphere material editing
- `SceneRenderer::GetSpecification()` mutable ref for runtime parameter tuning

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
2. ✅ Add a second demo — Material Playground (5 spheres, per-sphere Blinn-Phong material editing via ImGui)

Continue with:

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

### Phase 4 — Multi-Backend Refactor (Active)

Phases 0 and 3 are complete. Phase 1 (Layer 1-2) and Phase 2 (model loading, normal mapping, skybox)
remain in progress but are not blocking — they operate on the renderer/demo layer while Phase 4
operates on the graphics abstraction layer.

#### R1: Abstract Interface Layer (Active)

**Goal**: Define pure virtual interfaces so the renderer layer can be written against abstractions
instead of concrete OpenGL types. R1 only *defines* the interfaces — it does not refactor existing
classes to implement them (that is R2).

**Directory structure** (see also `renderTarget_design.md` §13):

```
src/graphics/
  interface/           ← NEW: pure virtual headers only (no platform #include)
    ITexture2D.h
    IVertexBuffer.h
    IIndexBuffer.h
    IVertexArray.h
    IShader.h
    IFramebuffer.h
    IRenderTarget.h
    IRenderCommand.h
    IGraphicsDevice.h
    GraphicsInterfaces.h   ← convenience header, includes all above
  opengl/              ← created in R2: GL implementations
  [root]               ← backend-agnostic: enums, specs, layouts, Material, Mesh
```

##### Step 0 — Spec cleanup (prerequisite)

Shared headers currently leak OpenGL types. These must be replaced with backend-agnostic
enums before the interfaces can be clean.

| File | Issue | Fix |
|------|-------|-----|
| `Texture.h` | `#include <glad/glad.h>`; `GLenum WrapS/WrapT/MinFilter/MagFilter` in `TextureSpecification` | Add `TextureWrap` / `TextureFilter` enums; remove glad from header |
| `Buffers.h` | `#include <glad/glad.h>`; inline `ToOpenGLBufferUsage()` | Move GL helper to `.cpp`; remove glad from header |

##### Interfaces

| Interface | Header | Key methods | Notes |
|-----------|--------|-------------|-------|
| `ITexture2D` | `ITexture2D.h` | `GetWidth`, `GetHeight`, `GetFormat`, `Bind(slot)`, `Unbind(slot)`, `SetData` | Leaf dependency — referenced by IFramebuffer, IRenderTarget |
| `IVertexBuffer` | `IVertexBuffer.h` | `SetData`, `SetLayout`, `GetLayout` | References `BufferLayout` (backend-agnostic) |
| `IIndexBuffer` | `IIndexBuffer.h` | `GetCount` | Minimal |
| `IVertexArray` | `IVertexArray.h` | `Bind`, `Unbind`, `AddVertexBuffer`, `SetIndexBuffer`, `GetIndexBuffer` | Metal/Vulkan have no VAO — impl stores bindings and applies at draw time |
| `IShader` | `IShader.h` | `Bind`, `Unbind`, `GetName`, `Set{Int,Float,Float2..4,Mat3,Mat4,Bool,IntArray}` | Full uniform setter surface — used by `Material::UploadToShader` and all passes |
| `IFramebuffer` | `IFramebuffer.h` | `Bind`, `Unbind`, `Resize`, `GetSpecification`, `GetColor/DepthAttachment`, `ReadPixel`, `ClearAttachment` | `FramebufferSpecification` stays in graphics root |
| `IRenderTarget` | `IRenderTarget.h` | `Bind`, `Unbind`, `Resize`, `GetWidth/Height`, `IsBackBuffer`, `GetColor/DepthAttachment` | Design from `renderTarget_design.md` §13.3 |
| `IRenderCommand` | `IRenderCommand.h` | `Init`, `SetClearColor`, `Clear`, `SetViewport`, `Enable{DepthTest,Blend,CullFace}`, `DrawIndexed`, `DrawArrays` | Current static `RenderCommand` becomes a forwarding shim in R2 |
| `IGraphicsDevice` | `IGraphicsDevice.h` | Factory: `CreateVertexBuffer`, `CreateIndexBuffer`, `CreateVertexArray`, `CreateTexture2D`, `CreateShader*`, `CreateFramebuffer`, `CreateRenderTarget*`, `GetRenderCommand` | Replaces all static `Create`/`CreateFromFile` methods. Global accessor `GetDevice()` set at Application init |

##### What does NOT need an interface

| Type | Reason |
|------|--------|
| `ShaderDataType`, `BufferElement`, `BufferLayout` | Pure CPU-side data description |
| `BufferUsage`, `TextureFormat`, `TextureSlot` | Backend-agnostic enums |
| `TextureWrap`, `TextureFilter` (new) | Backend-agnostic enums replacing `GLenum` |
| `FramebufferSpecification` and related | Config structs, no GPU calls |
| `Material` | Already backend-agnostic data container (Model B — see `material_system_design.md`). `UploadToShader(Ref<Shader>)` becomes `UploadToShader(Ref<IShader>)` in R2 |
| `Mesh` / `MeshFactory` | Concrete, but member types change to `Ref<IVertexArray>` etc. in R2. No virtual interface needed |
| `IUniformBuffer` | No UBO in codebase yet. Add interface when implementing multiple light sources (Phase 5) |

##### Design decisions

- **I-prefix**: consistent with `renderTarget_design.md` §13 and existing convention
- **No `GetRendererID()`** on any interface: GL-specific. OpenGL implementations keep it as a non-virtual method
- **`IGraphicsDevice` as factory hub**: single point of backend-specific object creation, set once at startup
- **`IRenderCommand` is instanced**: replaces current static-only `RenderCommand`. Existing static class can forward to the active instance during R2 transition

##### Implementation order

| Order | Task | Dependencies |
|-------|------|--------------|
| 0 | Spec cleanup (remove `GLenum` from `TextureSpecification`, move `ToOpenGLBufferUsage`) | None |
| 1 | `ITexture2D` | None (leaf) |
| 2 | `IVertexBuffer`, `IIndexBuffer` | `BufferLayout` |
| 3 | `IVertexArray` | `IVertexBuffer`, `IIndexBuffer` |
| 4 | `IShader` | glm, std::string |
| 5 | `IFramebuffer` | `ITexture2D`, `FramebufferSpecification` |
| 6 | `IRenderTarget` | `ITexture2D` |
| 7 | `IRenderCommand` | `IVertexArray` |
| 8 | `IGraphicsDevice` | All above |
| 9 | `GraphicsInterfaces.h` (convenience include) | All above |

##### Exit criteria

- All interface headers compile independently (no platform `#include`)
- `Texture.h` and `Buffers.h` no longer `#include <glad/glad.h>`
- `ctest` green (77/77)
- `grep -r "glad/glad.h" src/graphics/interface/` returns 0 results

#### R2: OpenGL Backend Reorganization

Move existing OpenGL code into `src/graphics/opengl/` implementing the R1 interfaces.
Mostly mechanical file moves + adding `override` on virtual methods.

- Rename `Texture2D` → `GLTexture2D : public ITexture2D`
- Rename `VertexBuffer` → `GLVertexBuffer : public IVertexBuffer`
- Rename `IndexBuffer` → `GLIndexBuffer : public IIndexBuffer`
- Rename `VertexArray` → `GLVertexArray : public IVertexArray`
- Rename `Shader` → `GLShader : public IShader`
- Rename `Framebuffer` → `GLFramebuffer : public IFramebuffer`
- Rename `RenderTarget` → `GLRenderTarget : public IRenderTarget`
- Rename `RenderCommand` → `GLRenderCommand : public IRenderCommand`
- Implement `GLGraphicsDevice : public IGraphicsDevice`
- Update all renderer / demo / test `#include` paths
- `Material::UploadToShader` changes parameter type to `Ref<IShader>`
- `Mesh` / `MeshFactory` use `Ref<IVertexArray>`, `Ref<IVertexBuffer>`, `Ref<IIndexBuffer>`

#### R3: Shader Pipeline (SPIR-V)

GLSL → SPIR-V (via glslang/shaderc, offline CMake step) → SPIRV-Cross for per-backend
transpilation. Single shader source, multiple backends.

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

- Metal / Vulkan backend implementations (R4/R5 — after R1-R3)
- SPIR-V shader pipeline (R3 — after R2)
- Render graph (Phase 5+)
- Backend-aware resource cache (Phase 5+)

These are valid long-term targets. Current focus is R1 (abstract interfaces) followed by R2 (OpenGL backend reorganization).
