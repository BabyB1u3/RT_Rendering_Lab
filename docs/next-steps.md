# Next Steps — Project Analysis & Development Plan

This document provides a comprehensive analysis of the current state of the RT Rendering Lab project,
covering both **feature development direction** and **architecture-level improvements**,
with prioritized recommendations for the next phase of development.

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
      - [3. Multiple Light Sources (Point Lights, Spot Lights)](#3-multiple-light-sources-point-lights-spot-lights)
      - [4. Skybox / Environment Map](#4-skybox--environment-map)
      - [5. Material Parameter UI](#5-material-parameter-ui)
    - [Post-Phase 2 Roadmap Adjustment](#post-phase-2-roadmap-adjustment)
  - [Part 2 — Architecture Improvements](#part-2--architecture-improvements)
    - [P0: Fix Immediately](#p0-fix-immediately)
      - [1. Add OpenGL Debug Callback](#1-add-opengl-debug-callback)
      - [2. Add Framebuffer Completeness Validation](#2-add-framebuffer-completeness-validation)
    - [P1: Before or During Phase 2 Completion](#p1-before-or-during-phase-2-completion)
      - [3. Uniform Buffer Object (UBO) Support](#3-uniform-buffer-object-ubo-support)
      - [4. Material Property System](#4-material-property-system)
      - [5. Extract Configuration from Hardcoded Values](#5-extract-configuration-from-hardcoded-values)
    - [P2: Before Phase 3](#p2-before-phase-3)
      - [6. Resource Cache (Shader / Texture)](#6-resource-cache-shader--texture)
      - [7. Unify RenderPass Interface](#7-unify-renderpass-interface)
      - [8. Fix Camera Pointer in SceneData](#8-fix-camera-pointer-in-scenedata)
    - [P3: When Needed](#p3-when-needed)
      - [9. Render Graph](#9-render-graph)
      - [10. Forward Declarations in Headers](#10-forward-declarations-in-headers)
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
      - [Stub File Targets (from Part 3)](#stub-file-targets-from-part-3)

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
- [ ] Multiple light sources (point, spot)
- [ ] Skybox / environment map
- [ ] Material parameter UI

---

## Part 1 — Feature Development Plan

### Phase 2 Remaining Items

Recommended implementation order, based on dependency relationships:

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

#### 3. Multiple Light Sources (Point Lights, Spot Lights)

Currently there is only a single directional light. Extending to multiple lights:

- Introduce `PointLight` / `SpotLight` data structures
- Pass light arrays via UBO or loop in shader
- Directly sets up the motivation for Phase 3's Deferred Rendering (where multi-light is the core advantage)

#### 4. Skybox / Environment Map

- Load a cubemap texture and render a skybox
- Prerequisite for Phase 4's IBL (Image-Based Lighting)
- Simple to implement, immediate visual improvement

#### 5. Material Parameter UI

- Expose material properties (diffuse color, specular intensity, roughness, etc.) via ImGui
- Useful for debugging and tuning
- Lays groundwork for the PBR material editor in Phase 4

---

### Post-Phase 2 Roadmap Adjustment

Rather than following Phases 3→4→5 linearly, a dependency-driven order is more efficient:

| Order | Content                      | Rationale                                                                 |
|-------|------------------------------|---------------------------------------------------------------------------|
| A     | HDR + Tone Mapping + Gamma   | Foundation of Phase 3; prerequisite for PBR — without HDR, high dynamic range in PBR cannot be displayed |
| B     | PBR (Cook-Torrance BRDF)     | With HDR pipeline + model loading + normal mapping, PBR follows naturally |
| C     | Deferred Rendering           | With multi-light + PBR in place, deferred rendering immediately shows its performance advantage |
| D     | SSAO                         | Deferred's G-Buffer natively provides the normal and depth data SSAO needs |
| E     | Bloom                        | Trivial to add on top of an HDR pipeline, high visual payoff              |

**Key insight**: HDR should come before PBR (not after), because PBR's physically correct intensity values
only make visual sense in an HDR pipeline with proper tone mapping.

---

## Part 2 — Architecture Improvements

### P0: Fix Immediately

These are low-effort, high-impact fixes that should be done before any new feature work.

#### 1. Add OpenGL Debug Callback

**Problem**: The entire project has no GL error checking beyond shader compilation exceptions.
All other GL errors are silently swallowed — texture format mismatches, incomplete framebuffers,
invalid uniform locations, etc. produce no feedback.

**Solution**: Register `glDebugMessageCallback` during initialization:

```cpp
// In Application or Window initialization
glEnable(GL_DEBUG_OUTPUT);
glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    LOG_WARN("GL Debug [{}]: {}", type, message);
}, nullptr);
```

**Effort**: ~20 lines. **Impact**: Catches all GL-level bugs automatically.

#### 2. Add Framebuffer Completeness Validation

**Problem**: `Framebuffer::Invalidate()` creates attachments but never verifies the framebuffer
is actually complete. An incomplete framebuffer renders nothing — silently.

**Solution**: Add validation after framebuffer creation:

```cpp
void Framebuffer::Validate() const
{
    GLenum status = glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Framebuffer incomplete: " + std::to_string(status));
}
```

**Effort**: ~10 lines. **Impact**: Prevents silent rendering failures.

---

### P1: Before or During Phase 2 Completion

These improvements are prerequisites for multi-light and PBR work.

#### 3. Uniform Buffer Object (UBO) Support

**Problem**: All uniforms are set individually per-frame via `SetMat4` / `SetFloat3` calls:

```cpp
// ForwardPass.cpp — repeated every frame, every item
m_Shader->SetMat4("u_ViewProjection", ...);
m_Shader->SetMat4("u_LightViewProjection", ...);
m_Shader->SetFloat3("u_CameraPosition", ...);
m_Shader->SetFloat3("u_LightDirection", ...);
m_Shader->SetFloat3("u_LightColor", ...);
m_Shader->SetFloat("u_LightIntensity", ...);
```

Per-frame data (camera, lights) is the same for all objects but is re-uploaded per draw call.
With multiple lights, this becomes a maintenance and performance bottleneck.

**Solution**: Introduce UBO abstraction for shared per-frame data:

```cpp
// Camera UBO — updated once per frame, shared across all shaders
struct CameraUBO {
    glm::mat4 ViewProjection;
    glm::vec3 Position;
};

// Light UBO — updated once per frame
struct LightUBO {
    DirectionalLight Sun;
    PointLight Points[MAX_POINT_LIGHTS];
    int PointLightCount;
};
```

**Effort**: Medium. **Impact**: Enables multi-light; cleaner shader interface.

#### 4. Material Property System

**Problem**: `Material` currently only manages texture bindings — no shader parameters:

```cpp
class Material {
    Ref<Shader> m_Shader;
    std::unordered_map<uint32_t, Ref<Texture2D>> m_Textures;  // Textures only
};
```

Diffuse color, specular intensity, roughness, and all other material properties are hardcoded
in Pass code. When PBR arrives, every object will have distinct material parameters.

**Solution**: Add a property system to Material:

```cpp
class Material {
    // Existing
    Ref<Shader> m_Shader;
    std::unordered_map<uint32_t, Ref<Texture2D>> m_Textures;

    // New: typed property storage
    std::unordered_map<std::string, float>     m_Floats;
    std::unordered_map<std::string, glm::vec3> m_Vec3s;
    std::unordered_map<std::string, glm::vec4> m_Vec4s;

    void UploadToShader();  // Binds all properties to the shader
};
```

**Effort**: Medium. **Impact**: Enables per-object material variation; essential for PBR.

#### 5. Extract Configuration from Hardcoded Values

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

### P2: Before Phase 3

These become important as the number of passes and resources grows.

#### 6. Resource Cache (Shader / Texture)

**Problem**: Each Pass independently loads its own shader with no deduplication:

```cpp
// ForwardPass.cpp
m_Shader = Shader::CreateFromSingleFile("assets/shaders/ForwardLit.glsl", "ForwardLit");
// ShadowPass.cpp
m_Shader = Shader::CreateFromSingleFile("assets/shaders/ShadowDepth.glsl", "ShadowDepth");
// TexturePreviewPass.cpp
m_Shader = Shader::CreateFromSingleFile("assets/shaders/TexturePreview.glsl", "TexturePreview");
```

Fallback resources (e.g., `m_FallbackShadowMap` in ForwardPass) are also created per-pass
rather than shared.

**Solution**: A lightweight cache keyed by file path:

```cpp
class ShaderCache {
public:
    static Ref<Shader> Get(const std::string &path, const std::string &name) {
        auto it = s_Cache.find(path);
        if (it != s_Cache.end()) return it->second;
        auto shader = Shader::CreateFromSingleFile(path, name);
        s_Cache[path] = shader;
        return shader;
    }
    static void Clear() { s_Cache.clear(); }
private:
    static inline std::unordered_map<std::string, Ref<Shader>> s_Cache;
};
```

Same pattern applies to textures. Fallback textures (white, black, normal-default) should
be created once and shared globally.

**Effort**: Low–Medium. **Impact**: Prevents duplicate GPU resources; simplifies resource management.

#### 7. Unify RenderPass Interface

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

This forces `SceneRenderer::Render()` to hardcode the call sequence and parameter
wiring for every pass. Adding a new pass always requires modifying the renderer.

**Solution**: Introduce a shared `RenderContext` that all passes read from:

```cpp
struct RenderContext {
    const SceneData *Scene = nullptr;
    glm::mat4 LightViewProjection;

    // Shared resource slots — passes write outputs, later passes read them
    std::unordered_map<std::string, Ref<Texture2D>> Textures;
    std::unordered_map<std::string, Ref<Framebuffer>> Framebuffers;
};

class RenderPass {
public:
    virtual void Execute(RenderContext &ctx) = 0;
    virtual void Resize(unsigned int width, unsigned int height) = 0;
};
```

Passes declare what they produce and consume via the context. The renderer
iterates a list of passes without knowing their internals.

**Effort**: Medium. **Impact**: Enables adding passes without modifying the renderer.

#### 8. Fix Camera Pointer in SceneData

**Problem**: `SceneData` holds a raw non-owning pointer to Camera:

```cpp
struct SceneData {
    Camera *ActiveCamera = nullptr;  // Dangling pointer risk
    DirectionalLight MainDirectionalLight;
    std::vector<RenderItem> RenderItems;
};
```

If the demo's camera is destroyed before the renderer finishes using SceneData,
this becomes a dangling pointer.

**Solution**: Use a const reference, or `Ref<Camera>` if shared ownership is needed:

```cpp
// Option A: Non-nullable reference wrapper
struct SceneData {
    std::reference_wrapper<const Camera> ActiveCamera;
    // ...
};

// Option B: Shared ownership
struct SceneData {
    Ref<Camera> ActiveCamera;
    // ...
};
```

**Effort**: Low. **Impact**: Eliminates a class of potential crashes.

---

### P3: When Needed

These are longer-term improvements that become relevant in Phase 3+ (Deferred Rendering and beyond).

#### 9. Render Graph

**Problem**: With deferred rendering, the pass chain grows significantly:

```
ShadowPass → G-Buffer Pass → Lighting Pass → SSAO → Bloom → Tone Mapping → Final
```

Manual orchestration in `SceneRenderer::Render()` becomes fragile —
resource dependencies between passes are implicit and error-prone.

**Solution**: A lightweight render graph where passes declare inputs and outputs,
and the framework automatically resolves execution order and resource lifetimes:

```cpp
class RenderGraph {
public:
    void AddPass(const std::string &name, Ref<RenderPass> pass,
                 std::vector<std::string> inputs,
                 std::vector<std::string> outputs);
    void Compile();   // Topological sort, allocate transient resources
    void Execute(const SceneData &scene);
};

// Usage
graph.AddPass("shadow",   shadowPass,   {},              {"ShadowMap"});
graph.AddPass("forward",  forwardPass,  {"ShadowMap"},   {"SceneColor", "SceneDepth"});
graph.AddPass("bloom",    bloomPass,    {"SceneColor"},  {"BloomResult"});
graph.AddPass("tonemap",  tonemapPass,  {"SceneColor", "BloomResult"}, {"FinalColor"});
```

**When**: Implement when transitioning to deferred rendering (Phase 3).
Before that, the manual approach with a unified `RenderContext` (P2-7) is sufficient.

**Effort**: High. **Impact**: Scalable pass management for complex pipelines.

#### 10. Forward Declarations in Headers

**Problem**: Headers like `Material.h` pull in `Shader.h` and `Texture.h`,
which in turn include `<glad/glad.h>` and `<glm/glm.hpp>`.
This creates a heavy transitive include chain.

**Solution**: Use forward declarations in headers, move includes to `.cpp`:

```cpp
// Material.h — before
#include "Shader.h"
#include "Texture.h"

// Material.h — after
class Shader;
class Texture2D;
enum class TextureSlot : uint32_t;
```

**When**: When compile times become noticeable.

**Effort**: Low. **Impact**: Faster incremental builds.

---

## Part 3 — Placeholder Files Implementation Guide

The project contains a number of empty stub files (0 bytes) that were pre-created as part of the intended architecture.
This section documents what each one should eventually contain, and **when** to implement it.

### Stub File Inventory

| File | Status | When to Implement |
|------|--------|-------------------|
| `src/core/FileSystem.h` / `.cpp` | Empty stub | P2 — Before Phase 3 |
| `src/graphics/RenderTarget.h` / `.cpp` | Empty stub | P2 — Before Phase 3 |
| `src/gui/ImGuiLayer.h` / `.cpp` | Empty stub | P1 — During Phase 2 |
| `src/gui/Panels/DebugPanel.h` / `.cpp` | Empty stub | P1 — During Phase 2 |
| `src/gui/Panels/DemoSelectorPanel.h` / `.cpp` | Empty stub | P1 — During Phase 2 |
| `archive/ResourceManager.h` / `.cpp` | Implemented in archive (old style) | Superseded by P2-6 |

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
- When the `DemoSelectorPanel` (P1) needs to route keyboard shortcuts to the active demo without
  the active demo polling directly
- When `ImGuiLayer` needs to consume input before passing it to the scene layer
- When demos need to register keybindings declaratively (e.g., `OnKeyPressed(Key::F, ToggleWireframe)`)

**Recommended design** (implement during P1, alongside ImGuiLayer):

```cpp
// src/core/KeyCode.h
// Thin enum wrapping GLFW key codes — decouples application from GLFW headers
enum class Key : int {
    W = GLFW_KEY_W,
    A = GLFW_KEY_A,
    S = GLFW_KEY_S,
    D = GLFW_KEY_D,
    Escape = GLFW_KEY_ESCAPE,
    F1 = GLFW_KEY_F1,
    // ... add as needed
};
```

```cpp
// src/core/Event.h
// Minimal event types — only add what is actually used
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

The `Layer` base class already has `OnResize()`. A general `OnEvent(Event&)` virtual can be
added to `Layer` when the event system is introduced — `ImGuiLayer` would intercept mouse/keyboard
events and set `Handled = true`, preventing them from reaching scene layers.

**Decision**: Do **not** build the event system before P1. It is unnecessary for Phase 2 rendering work
and would be premature infrastructure. Build `KeyCode.h` first (10 lines, zero risk), then `Event.h`
when `ImGuiLayer` requires input routing.

---

### Resource Manager

**Files**: `archive/ResourceManager.h`, `archive/ResourceManager.cpp` (implemented, but not in active `src/`)

**Current state**: The archive version uses an old-style static singleton pattern with value-type storage
(`std::map<std::string, Shader>` — no smart pointers). It does not match the current architecture
which uses `Ref<T>` (`std::shared_ptr`) throughout.

**What to do**: **Do not port the archive version directly.** Instead, implement the lightweight
`ShaderCache` / `TextureCache` described in P2-6 (Resource Cache), which fits the current `Ref<T>`
convention. The archive ResourceManager is superseded by that plan.

The archive version is retained for reference only (to understand what keys/names were used historically).

**When**: P2 — before Phase 3, as described in P2-6. The shader cache is a prerequisite for
sharing fallback textures (white, black, normal-default) across passes.

---

### RenderTarget

**Files**: `src/graphics/RenderTarget.h`, `src/graphics/RenderTarget.cpp`

**Current state**: Completely empty. The project currently uses `Framebuffer` directly for all
off-screen rendering (shadow map FBO, etc.).

**What it should be**: A higher-level abstraction that packages a `Framebuffer` together with its
color and depth `Texture2D` attachments, providing a single object that a pass can both render into
and sample from:

```cpp
class RenderTarget {
public:
    static Ref<RenderTarget> Create(uint32_t width, uint32_t height, TextureFormat colorFormat);

    void Bind();
    void Unbind();
    void Resize(uint32_t width, uint32_t height);

    Ref<Texture2D> GetColorAttachment() const;
    Ref<Texture2D> GetDepthAttachment() const;

private:
    Ref<Framebuffer> m_Framebuffer;
    Ref<Texture2D>   m_ColorAttachment;
    Ref<Texture2D>   m_DepthAttachment;
};
```

**Why**: Currently `ShadowPass` manually manages the FBO and depth texture as separate members.
When deferred rendering introduces a G-Buffer (multiple color attachments + depth), a `RenderTarget`
abstraction becomes essential to avoid duplicating FBO management code across every pass.

**When**: P2 — implement alongside the unified `RenderPass` interface (P2-7). `RenderTarget` and
`RenderContext` together replace the current ad-hoc FBO management in each pass.

---

### FileSystem

**Files**: `src/core/FileSystem.h`, `src/core/FileSystem.cpp`

**Current state**: Completely empty. Asset paths are currently hardcoded strings relative to the
working directory (e.g., `"assets/shaders/ForwardLit.glsl"`).

**What it should provide**:

```cpp
class FileSystem {
public:
    // Returns the absolute path to a file under the assets directory
    static std::string GetAssetPath(const std::string &relativePath);

    // Returns the absolute path to the project root (useful for tests)
    static std::string GetRootPath();

    // Reads entire file to string (convenience wrapper over std::ifstream)
    static std::string ReadTextFile(const std::string &path);

    static bool Exists(const std::string &path);
};
```

**Why**: Hardcoded relative paths break as soon as the working directory changes (e.g., when running
tests from a different directory, or packaging the application). A `FileSystem` utility centralizes
path resolution so that only one place needs to know where assets live.

**When**: P2 — implement when the Resource Cache (P2-6) is built, since the cache is keyed by path.
Having `FileSystem::GetAssetPath()` at that point ensures paths are resolved consistently.
For now, relative paths work fine and this stub can remain empty.

---

### ImGuiLayer & Panels

**Files**:
- `src/gui/ImGuiLayer.h` / `.cpp`
- `src/gui/Panels/DebugPanel.h` / `.cpp`
- `src/gui/Panels/DemoSelectorPanel.h` / `.cpp`

**Current state**: All empty. ImGui is already linked as a dependency and used directly inside
demo files (e.g., `ShadowMapping.cpp` calls `ImGui::*` directly in its `OnImGuiRender()`).
There is also a `// TODO: Implement demo selector` comment in `LabLayer.cpp`.

**What each should contain**:

#### ImGuiLayer

A `Layer` subclass that owns the ImGui context initialization and the per-frame begin/end calls.
Currently these calls are likely spread across the application loop. Centralizing them here means
no individual demo needs to know about ImGui setup:

```cpp
class ImGuiLayer : public Layer {
public:
    void OnAttach() override;   // ImGui::CreateContext(), style setup, backend init
    void OnDetach() override;   // ImGui::DestroyContext()

    void Begin();  // ImGui::NewFrame() — called by Application before layer OnImGuiRender()
    void End();    // ImGui::Render() + draw data submission — called after all layers
};
```

#### DemoSelectorPanel

Implements the `TODO` in `LabLayer.cpp`. Renders a list or combo of available demos and
triggers switching the active demo:

```cpp
class DemoSelectorPanel {
public:
    void OnImGuiRender(const std::vector<std::string> &demoNames, int &selectedIndex);
};
```

#### DebugPanel

A general-purpose stats overlay (frame time, FPS, draw call count, active pass list).
Can also expose the `SceneRendererSpec` sliders (shadow map resolution, light frustum size)
for runtime tuning — directly complements the "Extract Configuration" item (P1-5).

**When**: P1 — these become useful as soon as the demo selector TODO needs to be resolved and
as material parameter UI (Phase 2 remaining item 5) is implemented. `ImGuiLayer` should be
built first as the other panels depend on it.

---

## Combined Priority Matrix

A unified view of all feature and architecture work, sorted by implementation order:

| Priority | Category     | Item                              | Effort | Blocked By    |
|----------|--------------|-----------------------------------|--------|---------------|
| **P1**   | Architecture | Extract Hardcoded Configuration   | Low    | —             |
| **P1**   | Architecture | UBO Support                       | Medium | —             |
| **P1**   | Architecture | Material Property System          | Medium | —             |
| **P1**   | Feature      | Model Loading (glTF)              | Medium | —             |
| **P1**   | Feature      | Normal Mapping                    | Medium | Model Loading  |
| **P1**   | Feature      | Multiple Light Sources            | Medium | UBO            |
| **P1**   | Feature      | Skybox / Environment Map          | Low    | —             |
| **P1**   | Feature      | Material Parameter UI             | Low    | Material Props |
| **P2**   | Architecture | Resource Cache                    | Low    | —             |
| **P2**   | Architecture | Unify RenderPass Interface        | Medium | —             |
| **P2**   | Architecture | Fix Camera Pointer                | Low    | —             |
| **P2**   | Feature      | HDR + Tone Mapping + Gamma        | Medium | —             |
| **P2**   | Feature      | PBR (Cook-Torrance)               | High   | HDR, Material  |
| **P2**   | Feature      | Bloom                             | Medium | HDR            |
| **P3**   | Architecture | Render Graph                      | High   | RenderPass I/F |
| **P3**   | Architecture | Forward Declarations in Headers   | Low    | —             |
| **P3**   | Feature      | Deferred Rendering                | High   | Render Graph   |
| **P3**   | Feature      | SSAO                              | Medium | Deferred       |

#### Stub File Targets (from Part 3)

| Priority | Category     | Stub File                         | Effort | Blocked By    |
|----------|--------------|-----------------------------------|--------|---------------|
| **P1**   | GUI          | ImGuiLayer.h / .cpp               | Low    | —             |
| **P1**   | GUI          | DemoSelectorPanel, DebugPanel     | Low    | ImGuiLayer    |
| **P1**   | Core         | KeyCode.h (enum only)             | Trivial| —             |
| **P2**   | Core         | Event.h / event routing           | Medium | ImGuiLayer    |
| **P2**   | Graphics     | RenderTarget.h / .cpp             | Medium | Framebuffer   |
| **P2**   | Core         | FileSystem.h / .cpp               | Low    | —             |