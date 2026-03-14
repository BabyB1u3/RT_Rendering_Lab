# Next Steps — Project Analysis & Development Plan

This document provides a comprehensive analysis of the current state of the RT Rendering Lab project,
covering both **feature development direction** and **architecture-level improvements**,
with prioritized recommendations for the next phase of development.

> **Architecture Goal Update**: The project target has shifted to **true cross-platform support**
> via a multi-backend architecture (OpenGL on Windows/Linux, Metal on macOS, Vulkan optional).
> All task priorities below reflect this updated direction.

---

## Table of Contents

- [Next Steps — Project Analysis \& Development Plan](#next-steps--project-analysis--development-plan)
  - [Table of Contents](#table-of-contents)
  - [Current State Summary](#current-state-summary)
    - [Phase 2 Completed](#phase-2-completed)
    - [Phase 2 Remaining](#phase-2-remaining)
  - [Part 1 — Feature Development Plan](#part-1--feature-development-plan)
    - [Phase 2 Remaining Items](#phase-2-remaining-items)
      - [1. Model Loading (Highest Priority)](#1-model-loading-highest-priority)
      - [2. Normal Mapping](#2-normal-mapping)
      - [3. Skybox / Environment Map](#3-skybox--environment-map)
      - [4. Material Parameter UI](#4-material-parameter-ui)
    - [Post-Refactor Feature Roadmap](#post-refactor-feature-roadmap)
  - [Part 2 — Architecture Plan](#part-2--architecture-plan)
    - [Stage 0: Pre-Refactor Tasks](#stage-0-pre-refactor-tasks)
      - [1. Extract Configuration from Hardcoded Values](#1-extract-configuration-from-hardcoded-values)
      - [2. Fix Camera Pointer in SceneData](#2-fix-camera-pointer-in-scenedata)
      - [3. Material Property System](#3-material-property-system)
      - [4. Implement RenderTarget](#4-implement-rendertarget)
      - [5. Unify RenderPass Interface](#5-unify-renderpass-interface)
    - [Stage 1: Multi-Backend Refactor](#stage-1-multi-backend-refactor)
      - [Phase R1: Abstract Interface Layer](#phase-r1-abstract-interface-layer)
      - [Phase R2: OpenGL Backend Reorganization](#phase-r2-opengl-backend-reorganization)
      - [Phase R3: Shader Pipeline Redesign (SPIR-V)](#phase-r3-shader-pipeline-redesign-spir-v)
      - [Phase R4: Metal Backend](#phase-r4-metal-backend)
      - [Phase R5: Vulkan Backend (Optional)](#phase-r5-vulkan-backend-optional)
    - [Stage 2: Post-Refactor Architecture](#stage-2-post-refactor-architecture)
      - [6. Backend-Aware Resource Cache](#6-backend-aware-resource-cache)
      - [7. Render Graph](#7-render-graph)
      - [8. Forward Declarations in Headers](#8-forward-declarations-in-headers)
  - [Part 3 — Placeholder Files Implementation Guide](#part-3--placeholder-files-implementation-guide)
    - [Stub File Inventory](#stub-file-inventory)
    - [KeyCode \& Event System](#keycode--event-system)
    - [Resource Manager](#resource-manager)
    - [RenderTarget](#rendertarget)
    - [FileSystem](#filesystem)
    - [ImGuiLayer \& Panels](#imguilayer--panels)
      - [ImGuiLayer](#imguilayer)
      - [DemoSelectorPanel](#demoselectorpanel)
      - [DebugPanel](#debugpanel)
  - [Combined Priority Matrix](#combined-priority-matrix)

---

## Current State Summary

| Metric              | Status                                              |
|---------------------|-----------------------------------------------------|
| Phase 1 (Framework) | Complete                                             |
| Phase 2 (Basic RT)  | ~60% — lighting, shadows, Blinn-Phong done           |
| Active Demos        | 1 (Shadow Mapping)                                   |
| Shader Files        | 3 (ForwardLit, ShadowDepth, TexturePreview)          |
| Source Code          | ~3,200 lines across 7 modules                        |
| Test Coverage        | Unit + Integration (Google Test)                     |
| Rendering Pipeline   | Forward: ShadowPass → ForwardPass → TexturePreview  |
| Platform Support     | Windows only (OpenGL 4.6 hardcoded)                  |
| Architecture Goal    | Multi-backend: OpenGL / Metal / Vulkan               |

### Phase 2 Completed

- [x] Forward rendering pass
- [x] Directional light with ambient + diffuse shading
- [x] Shadow mapping (depth pass, bias, 3×3 PCF)
- [x] Shadow map debug visualization
- [x] Multi-pass scene renderer
- [x] Blinn-Phong specular lighting
- [x] Normal matrix precomputed on CPU
- [x] Type-safe `enum class TextureSlot` binding

### Phase 2 Remaining

- [ ] Model loading (OBJ / glTF)
- [ ] Normal mapping
- [ ] Skybox / environment map
- [ ] Material parameter UI
- [ ] Multiple light sources (point, spot) — **moved to post-refactor** (depends on `IUniformBuffer`)

---

## Part 1 — Feature Development Plan

### Phase 2 Remaining Items

Recommended implementation order. These items are **backend-agnostic** enough to be done before or during
the multi-backend refactor. Multiple Light Sources is moved to post-refactor because it requires the
`IUniformBuffer` abstraction to be designed for all backends simultaneously.

#### 1. Model Loading (Highest Priority)

Currently the project only has procedural cube/plane meshes, which limits scene expressiveness.
Introducing **Assimp** or **tinygltf** to load glTF models would:

- Enable rendering of complex geometry to validate the pipeline
- Provide realistic test assets for normal mapping, PBR, and later features
- glTF natively carries PBR material parameters, enabling a smooth transition to Phase 4

#### 2. Normal Mapping

With model loading in place, normal mapping follows naturally:

- Requires constructing TBN matrices in the vertex shader
- Relatively small modification to `ForwardLit.glsl`
- One of the highest visual-impact-to-effort-ratio features

#### 3. Skybox / Environment Map

- Load a cubemap texture and render a skybox
- Prerequisite for Phase 4's IBL (Image-Based Lighting)
- Simple to implement, immediate visual improvement

#### 4. Material Parameter UI

- Expose material properties (diffuse color, specular intensity, roughness, etc.) via ImGui
- Useful for debugging and tuning
- Lays groundwork for the PBR material editor in Phase 4

---

### Post-Refactor Feature Roadmap

These features resume after the multi-backend refactor (Stage 1) is complete.

| Order | Content                      | Rationale                                                                 |
|-------|------------------------------|---------------------------------------------------------------------------|
| A     | Multiple Light Sources       | Requires `IUniformBuffer` to be consistent across all backends            |
| B     | HDR + Tone Mapping + Gamma   | Foundation prerequisite for PBR — without HDR, high dynamic range cannot be displayed |
| C     | PBR (Cook-Torrance BRDF)     | With HDR pipeline + model loading + normal mapping, PBR follows naturally |
| D     | Deferred Rendering           | With multi-light + PBR in place, deferred rendering immediately shows its performance advantage |
| E     | SSAO                         | Deferred's G-Buffer natively provides the normal and depth data SSAO needs |
| F     | Bloom                        | Trivial to add on top of an HDR pipeline, high visual payoff              |

---

## Part 2 — Architecture Plan

Architecture work is now organized into three stages aligned with the multi-backend refactoring goal.

---

### Stage 0: Pre-Refactor Tasks

These must be completed **before** beginning the multi-backend refactor.
They clean up the current codebase and establish the abstractions the refactor will build on.

#### 1. Extract Configuration from Hardcoded Values

**Problem**: Configuration values are scattered across source files:

| Location                | Hardcoded Value                                   |
|-------------------------|---------------------------------------------------|
| `SceneRenderer.cpp`     | Shadow map resolution `2048×2048`                  |
| `SceneRenderer.cpp`     | Light frustum: `orthoSize=10, near=0.1, far=30`   |
| Pass constructors       | Shader paths `"assets/shaders/ForwardLit.glsl"`    |
| `main.cpp`              | Window resolution `1600×900`                       |
| `ShadowMapping.cpp`     | Camera speed, sensitivity values                   |

**Solution**: Introduce configuration structs:

```cpp
struct SceneRendererSpec {
    uint32_t ShadowMapWidth  = 2048;
    uint32_t ShadowMapHeight = 2048;
    float    LightFrustumSize = 10.0f;
    float    LightNearPlane   = 0.1f;
    float    LightFarPlane    = 30.0f;
};
```

Each demo can then customize these values without modifying framework code.

**Effort**: Low. **Impact**: Cleaner separation between framework and demo-specific tuning.

---

#### 2. Fix Camera Pointer in SceneData

**Problem**: `SceneData` holds a raw non-owning pointer to Camera:

```cpp
struct SceneData {
    Camera *ActiveCamera = nullptr;  // Dangling pointer risk
    DirectionalLight MainDirectionalLight;
    std::vector<RenderItem> RenderItems;
};
```

**Solution**: Use a const reference, or `Ref<Camera>` if shared ownership is needed:

```cpp
// Option A: Non-nullable reference wrapper
struct SceneData {
    std::reference_wrapper<const Camera> ActiveCamera;
};

// Option B: Shared ownership
struct SceneData {
    Ref<Camera> ActiveCamera;
};
```

**Effort**: Low. **Impact**: Eliminates a class of potential crashes.

---

#### 3. Material Property System

**Problem**: `Material` currently only manages texture bindings — no shader parameters.
Diffuse color, specular intensity, roughness, and all other material properties are hardcoded
in Pass code. When PBR arrives, every object will have distinct material parameters.

**Solution**: Add a property system to Material:

```cpp
class Material {
    Ref<Shader> m_Shader;
    std::unordered_map<uint32_t, Ref<Texture2D>> m_Textures;

    // New: typed property storage
    std::unordered_map<std::string, float>     m_Floats;
    std::unordered_map<std::string, glm::vec3> m_Vec3s;
    std::unordered_map<std::string, glm::vec4> m_Vec4s;

    void UploadToShader();  // Binds all properties to the shader
};
```

This is a **data model design** that is backend-agnostic. The `UploadToShader()` implementation
will vary per backend, but the property storage itself is universal.

**Effort**: Medium. **Impact**: Enables per-object material variation; essential for PBR.

---

#### 4. Implement RenderTarget

**Problem**: `src/graphics/RenderTarget.h` / `.cpp` are empty stubs. Currently each pass manually
manages FBO binding and texture attachment access as separate members.

**Solution**: Implement the thin-wrapper `RenderTarget` as designed in `renderTarget_design.md`.
This wrapper packages a `Framebuffer` into a render-flow object that unifies backbuffer and offscreen
FBO under one interface — enabling Step 5 (Unify RenderPass Interface) to work.

In the multi-backend world, this thin wrapper becomes the **OpenGL backend's concrete implementation**
(`GLRenderTarget`). The abstract interface design is described in `renderTarget_design.md` Section 13.

**Effort**: Low–Medium. **Impact**: Prerequisite for unifying the RenderPass interface.

---

#### 5. Unify RenderPass Interface

**Problem**: The `RenderPass` base class only defines `Resize()`. Each pass has a different
`Execute()` signature, making polymorphic usage impossible:

```cpp
// Current
class RenderPass {
    virtual void Resize(unsigned int width, unsigned int height) = 0;
};

// ShadowPass::Execute(const SceneData &, const glm::mat4 &)
// ForwardPass::Execute(const SceneData &, Ref<Texture2D>, const glm::mat4 &)
// TexturePreviewPass::Execute(Ref<Texture2D>, bool)
```

This forces `SceneRenderer::Render()` to hardcode the call sequence for every pass.

**Solution**: Introduce a shared `RenderContext` that all passes read from:

```cpp
struct RenderContext {
    const SceneData *Scene = nullptr;
    glm::mat4 LightViewProjection;

    // Shared resource slots — passes write outputs, later passes read them
    std::unordered_map<std::string, Ref<Texture2D>> Textures;
    std::unordered_map<std::string, RenderTarget>   Targets;
};

class RenderPass {
public:
    virtual void Execute(RenderContext &ctx) = 0;
    virtual void Resize(unsigned int width, unsigned int height) = 0;
};
```

**Why this is critical for the refactor**: Once all passes share a unified `Execute(RenderContext&)`
signature, the multi-backend refactor only needs to re-implement the pass internals —
the `SceneRenderer` orchestration logic does not change. Without this, every backend would require
a separately wired renderer.

**Effort**: Medium. **Impact**: The single most important prerequisite for the multi-backend refactor.

---

### Stage 1: Multi-Backend Refactor

This is the main architecture work. Do not begin until all Stage 0 tasks are complete.

#### Phase R1: Abstract Interface Layer

Define pure virtual interfaces for all graphics resources. These live in `src/graphics/interface/`
and contain **no platform-specific includes**.

```
src/graphics/
  interface/
    IVertexBuffer.h
    IIndexBuffer.h
    IVertexArray.h
    ITexture2D.h
    IFramebuffer.h
    IRenderTarget.h
    IShader.h
    IUniformBuffer.h      ← replaces the old "UBO Support" task
    IRenderCommand.h
    IGraphicsDevice.h     ← factory that creates all other resources
```

> **Note on UBO**: The original P1 task "Uniform Buffer Object (UBO) Support" was OpenGL-specific.
> In the multi-backend world, uniform data is handled very differently per API:
> - OpenGL: UBO (`glBindBufferBase`)
> - Vulkan: push constants + descriptor sets
> - Metal: `setVertexBytes` / `MTLBuffer`
>
> `IUniformBuffer` is the abstract interface that maps to each backend's mechanism.
> **Do not implement a standalone UBO class before this phase.**

**Effort**: Medium. **Impact**: Foundation of the entire multi-backend system.

---

#### Phase R2: OpenGL Backend Reorganization

Move all existing OpenGL code into `src/graphics/opengl/`, implementing the interfaces from R1.

```
src/graphics/
  opengl/
    GLVertexBuffer.h / .cpp    (was Buffers.h / Buffers.cpp)
    GLIndexBuffer.h / .cpp
    GLVertexArray.h / .cpp
    GLTexture2D.h / .cpp       (was Texture.h / .cpp)
    GLFramebuffer.h / .cpp     (was Framebuffer.h / .cpp)
    GLRenderTarget.h / .cpp    (was RenderTarget.h / .cpp, now OpenGL-specific)
    GLShader.h / .cpp          (was Shader.h / .cpp)
    GLUniformBuffer.h / .cpp   (new — wraps UBO)
    GLRenderCommand.h / .cpp
    GLGraphicsDevice.h / .cpp  (factory)
```

**Effort**: Medium (mostly mechanical file moves + adding virtual overrides). **Impact**: Existing code
continues to work; OpenGL is now one swappable backend.

---

#### Phase R3: Shader Pipeline Redesign (SPIR-V)

**Problem**: All shaders are GLSL 4.6. Metal requires MSL; maintaining two shader files per effect
is a maintenance trap.

**Solution**: GLSL → SPIR-V → SPIRV-Cross:

```
assets/shaders/ForwardLit.glsl
    │
    ▼ glslang / shaderc (offline, CMake step)
    │
assets/shaders/compiled/ForwardLit.spv
    │
    ├─ OpenGL backend:  SPIRV-Cross → GLSL 4.1 (or inject version header)
    ├─ Metal backend:   SPIRV-Cross → MSL
    └─ Vulkan backend:  consume SPIR-V directly
```

Dependencies to add:
- `glslang` or `shaderc` — offline GLSL → SPIR-V compilation (CMake build step)
- `SPIRV-Cross` — runtime SPIR-V → target language transpilation

**Effort**: Medium–High. **Impact**: Single source of truth for all shaders; enables Metal.

---

#### Phase R4: Metal Backend

Implement Metal backend using **metal-cpp** (Apple's official C++ header-only wrapper).
Files live in `src/graphics/metal/` with `.mm` extension for Objective-C++ linkage.

```
src/graphics/
  metal/
    MTLVertexBuffer.h / .mm
    MTLTexture2D.h / .mm
    MTLFramebuffer.h / .mm     (wraps MTLRenderPassDescriptor)
    MTLRenderTarget.h / .mm
    MTLShader.h / .mm          (receives MSL from SPIRV-Cross)
    MTLUniformBuffer.h / .mm   (setVertexBytes / MTLBuffer)
    MTLRenderCommand.h / .mm
    MTLGraphicsDevice.h / .mm
```

CMake selects the backend at configure time:
```cmake
if(APPLE)
    target_sources(... PRIVATE src/graphics/metal/...)
    find_library(METAL_LIB Metal REQUIRED)
    target_link_libraries(... ${METAL_LIB})
else()
    target_sources(... PRIVATE src/graphics/opengl/...)
    target_link_libraries(... glad)
endif()
```

**Effort**: High. **Impact**: True macOS support with full feature parity.

---

#### Phase R5: Vulkan Backend (Optional)

Vulkan consumes SPIR-V directly (no transpilation needed), making it the simplest backend
to add once R3 is complete. Primarily useful for Windows/Linux if OpenGL is to be phased out.

**When**: After R4 is stable. **Effort**: High. **Impact**: Future-proof, enables ray tracing extensions.

---

### Stage 2: Post-Refactor Architecture

These become relevant after the multi-backend refactor is in place.

#### 6. Backend-Aware Resource Cache

**Problem**: Shader and texture loading should be deduplicated, but in a multi-backend world,
a "shader" is not a single object — it is a SPIR-V blob + per-backend compiled artifact.

**Solution**: A cache that is keyed by asset path but stores backend-specific compiled resources:

```cpp
class ShaderLibrary {
public:
    // Loads SPIR-V, transpiles for current backend, caches result
    static Ref<IShader> Get(const std::string &spvPath);
    static void Clear();
private:
    static inline std::unordered_map<std::string, Ref<IShader>> s_Cache;
};
```

**Effort**: Low–Medium. **Impact**: Prevents duplicate GPU resources; must be backend-aware.

---

#### 7. Render Graph

**Problem**: With deferred rendering, the pass chain grows significantly:

```
ShadowPass → G-Buffer Pass → Lighting Pass → SSAO → Bloom → Tone Mapping → Final
```

Manual orchestration in `SceneRenderer::Render()` becomes fragile.

**Solution**: A lightweight render graph where passes declare inputs and outputs,
and the framework automatically resolves execution order and resource lifetimes.
The `RenderContext` introduced in Stage 0 is the foundation for this.

**When**: Implement when transitioning to deferred rendering.

**Effort**: High. **Impact**: Scalable pass management for complex pipelines.

---

#### 8. Forward Declarations in Headers

**Problem**: Headers like `Material.h` pull in `Shader.h` and `Texture.h`,
which in turn include platform-specific headers. With multiple backends, this
transitive include chain becomes more problematic.

**Solution**: Use forward declarations in headers, move includes to `.cpp`.

**When**: When compile times become noticeable.

**Effort**: Low. **Impact**: Faster incremental builds.

---

## Part 3 — Placeholder Files Implementation Guide

### Stub File Inventory

| File | Status | When to Implement |
|------|--------|-------------------|
| `src/core/FileSystem.h` / `.cpp` | Empty stub | Stage 0 — Before Refactor |
| `src/graphics/RenderTarget.h` / `.cpp` | Empty stub | Stage 0 — Before Refactor (OpenGL thin wrapper) |
| `src/gui/ImGuiLayer.h` / `.cpp` | Empty stub | Stage 0 — Before Refactor |
| `src/gui/Panels/DebugPanel.h` / `.cpp` | Empty stub | Stage 0 — Before Refactor |
| `src/gui/Panels/DemoSelectorPanel.h` / `.cpp` | Empty stub | Stage 0 — Before Refactor |
| `archive/ResourceManager.h` / `.cpp` | Implemented in archive (old style) | Superseded by Stage 2 Resource Cache |

> **Note**: There is no `KeyCode.h` or `Event.h` stub in the active codebase.
> These systems do not yet exist and are not planned for Phase 2.
> See the [KeyCode & Event System](#keycode--event-system) section below for the design rationale.

---

### KeyCode & Event System

**Files**: None exist yet. No stubs have been created.

**Current state**: Input is handled by `src/core/Input.h` via direct GLFW polling (`glfwGetKey`, `glfwGetMouseButton`).
Raw GLFW key constants (e.g., `GLFW_KEY_W`) are used directly at call sites. Window resize is
delivered via a registered callback on the `Window` object.

**Why it hasn't been built yet**: For a rendering lab focused on GPU features, an event system
is infrastructure overhead. Direct polling is sufficient when there is one window, one demo at a time,
and no need for UI input routing.

**When it becomes necessary**:
- When the `DemoSelectorPanel` needs to route keyboard shortcuts to the active demo
- When `ImGuiLayer` needs to consume input before passing it to the scene layer
- When demos need to register keybindings declaratively

**Recommended design** (implement during Stage 0, alongside ImGuiLayer):

```cpp
// src/core/KeyCode.h
enum class Key : int {
    W = GLFW_KEY_W,
    A = GLFW_KEY_A,
    S = GLFW_KEY_S,
    D = GLFW_KEY_D,
    Escape = GLFW_KEY_ESCAPE,
    F1 = GLFW_KEY_F1,
};
```

```cpp
// src/core/Event.h
enum class EventType { WindowResize, KeyPressed, KeyReleased, MouseMoved };

struct Event {
    EventType Type;
    bool Handled = false;
};

struct KeyEvent : Event {
    Key Code;
    bool Repeat;
};

struct WindowResizeEvent : Event {
    unsigned int Width, Height;
};
```

**Decision**: Do **not** build the event system before ImGuiLayer. Build `KeyCode.h` first
(10 lines, zero risk), then `Event.h` when `ImGuiLayer` requires input routing.

---

### Resource Manager

**Files**: `archive/ResourceManager.h`, `archive/ResourceManager.cpp` (implemented, but not in active `src/`)

**Current state**: The archive version uses an old-style static singleton pattern with value-type storage
(`std::map<std::string, Shader>` — no smart pointers). It does not match the current architecture
which uses `Ref<T>` (`std::shared_ptr`) throughout.

**What to do**: **Do not port the archive version directly.** Instead, implement the backend-aware
`ShaderLibrary` / `TextureCache` described in Stage 2 (Post-Refactor), which will operate on
`IShader` and `ITexture2D` interfaces rather than OpenGL-specific types.
The archive version is retained for reference only.

**When**: Stage 2 — after the multi-backend refactor is complete.

---

### RenderTarget

**Files**: `src/graphics/RenderTarget.h`, `src/graphics/RenderTarget.cpp`

**Current state**: Completely empty. The project currently uses `Framebuffer` directly for all
off-screen rendering (shadow map FBO, etc.).

**Stage 0 implementation**: Implement the **thin-wrapper** `RenderTarget` as specified in
`renderTarget_design.md`. This version wraps an existing `Framebuffer` and provides a unified
interface for backbuffer vs offscreen targets. It will live directly in `src/graphics/` and
include `glad` in the `.cpp`.

**After refactor (Stage 1)**: The thin wrapper becomes `GLRenderTarget` under `src/graphics/opengl/`.
The abstract `IRenderTarget` (or `RenderTarget` as a pure virtual base class) lives in
`src/graphics/interface/`. Metal backend provides `MTLRenderTarget` that wraps `MTLRenderPassDescriptor`.

See `renderTarget_design.md` for the full design, including the multi-backend evolution in Section 13.

---

### FileSystem

**Files**: `src/core/FileSystem.h`, `src/core/FileSystem.cpp`

**Current state**: Completely empty. Asset paths are currently hardcoded strings relative to the
working directory (e.g., `"assets/shaders/ForwardLit.glsl"`).

**What it should provide**:

```cpp
class FileSystem {
public:
    static std::string GetAssetPath(const std::string &relativePath);
    static std::string GetRootPath();
    static std::string ReadTextFile(const std::string &path);
    static bool Exists(const std::string &path);
};
```

**Why**: Hardcoded relative paths break as soon as the working directory changes (e.g., when running
tests, or packaging). A `FileSystem` utility centralizes path resolution — especially important
for the SPIR-V shader loading pipeline in Stage 1.

**When**: Stage 0 — implement when beginning the RenderTarget or Resource Cache work.

---

### ImGuiLayer & Panels

**Files**:
- `src/gui/ImGuiLayer.h` / `.cpp`
- `src/gui/Panels/DebugPanel.h` / `.cpp`
- `src/gui/Panels/DemoSelectorPanel.h` / `.cpp`

**Current state**: All empty. ImGui is already linked as a dependency and used directly inside
demo files (e.g., `ShadowMapping.cpp` calls `ImGui::*` directly in its `OnImGuiRender()`).

#### ImGuiLayer

A `Layer` subclass that owns the ImGui context initialization and the per-frame begin/end calls:

```cpp
class ImGuiLayer : public Layer {
public:
    void OnAttach() override;   // ImGui::CreateContext(), style setup, backend init
    void OnDetach() override;   // ImGui::DestroyContext()

    void Begin();  // ImGui::NewFrame()
    void End();    // ImGui::Render() + draw data submission
};
```

#### DemoSelectorPanel

Implements the `TODO` in `LabLayer.cpp`. Renders a list or combo of available demos:

```cpp
class DemoSelectorPanel {
public:
    void OnImGuiRender(const std::vector<std::string> &demoNames, int &selectedIndex);
};
```

#### DebugPanel

A general-purpose stats overlay (frame time, FPS, draw call count, active pass list).
Can also expose the `SceneRendererSpec` sliders for runtime tuning.

**When**: Stage 0 — `ImGuiLayer` should be built first as the other panels depend on it.

---

## Combined Priority Matrix

A unified view of all work, organized by the three stages:

### Stage 0: Pre-Refactor (Do First)

| Priority | Category     | Item                              | Effort | Blocked By    |
|----------|--------------|-----------------------------------|--------|---------------|
| **S0**   | Architecture | Extract Hardcoded Configuration   | Low    | —             |
| **S0**   | Architecture | Fix Camera Pointer                | Low    | —             |
| **S0**   | Architecture | Material Property System          | Medium | —             |
| **S0**   | Architecture | Implement RenderTarget            | Medium | Framebuffer   |
| **S0**   | Architecture | Unify RenderPass Interface        | Medium | RenderTarget  |
| **S0**   | Feature      | Model Loading (glTF)              | Medium | —             |
| **S0**   | Feature      | Normal Mapping                    | Medium | Model Loading  |
| **S0**   | Feature      | Skybox / Environment Map          | Low    | —             |
| **S0**   | Feature      | Material Parameter UI             | Low    | Material Props |
| **S0**   | GUI          | ImGuiLayer.h / .cpp               | Low    | —             |
| **S0**   | GUI          | DemoSelectorPanel, DebugPanel     | Low    | ImGuiLayer    |
| **S0**   | Core         | FileSystem.h / .cpp               | Low    | —             |
| **S0**   | Core         | KeyCode.h (enum only)             | Trivial| —             |
| **S0**   | Core         | Event.h / event routing           | Medium | ImGuiLayer    |

### Stage 1: Multi-Backend Refactor

| Priority | Phase  | Item                              | Effort | Blocked By    |
|----------|--------|-----------------------------------|--------|---------------|
| **R1**   | Refactor | Abstract Interface Layer        | Medium | Stage 0 complete |
| **R2**   | Refactor | OpenGL Backend Reorganization   | Medium | R1            |
| **R3**   | Refactor | Shader Pipeline (SPIR-V)        | High   | R2            |
| **R4**   | Refactor | Metal Backend                   | High   | R3            |
| **R5**   | Refactor | Vulkan Backend (optional)       | High   | R3            |

### Stage 2: Post-Refactor

| Priority | Category     | Item                              | Effort | Blocked By    |
|----------|--------------|-----------------------------------|--------|---------------|
| **S2**   | Feature      | Multiple Light Sources            | Medium | IUniformBuffer |
| **S2**   | Feature      | HDR + Tone Mapping + Gamma        | Medium | —             |
| **S2**   | Feature      | PBR (Cook-Torrance)               | High   | HDR, Material  |
| **S2**   | Feature      | Bloom                             | Medium | HDR            |
| **S2**   | Architecture | Backend-Aware Resource Cache      | Medium | R1            |
| **S2**   | Architecture | Render Graph                      | High   | RenderPass I/F |
| **S2**   | Architecture | Forward Declarations in Headers   | Low    | —             |
| **S3**   | Feature      | Deferred Rendering                | High   | Render Graph   |
| **S3**   | Feature      | SSAO                              | Medium | Deferred       |
