# Shader & Material System — Architecture and Evolution Plan

This document is the single source of truth for the shader pipeline and material system design.
It describes the current state, the target architecture, and the evolution path between them.

It supersedes:
- `shader-vulkan-migration.md` (Vulkan resource model migration — now handled by Slang)
- `material_system_design.md` (Material system evolution — merged here)

For the Slang migration itself (toolchain swap, build system, shader rewrites), see `slang-migration.md`.

---

## 1. Shader Pipeline Architecture

### 1.1 Pre-Migration State (R3a — Current)

```
Source:     GLSL (.vert / .frag) — bare uniforms, no UBOs
Compile:    glslang -G --auto-map-locations --auto-map-bindings → SPIR-V
Transpile:  SPIRV-Cross (runtime) → GLSL 460 (strip plain uniform bindings)
Backend:    OpenGL 4.6 only
```

Limitations:
- Bare `uniform` declarations are incompatible with Vulkan/Metal
- Runtime transpilation adds startup cost and a fragile workaround layer
- No module system, no variant management
- Name-based uniform setters (`SetMat4("u_Model", ...)`) only work for OpenGL

### 1.2 Post-Migration Target (R3b)

```
Source:     Slang (.slang) — structured resources (ParameterBlock, cbuffer)
                             module imports, interface/generics for variants
Compile:    slangc (build-time) → per-backend artifacts:
                ├── GLSL 460 source   (OpenGL)
                ├── SPIR-V binary     (Vulkan)
                └── MSL source        (Metal)
Backend:    OpenGL 4.6, Vulkan 1.3, Metal 3 (incremental)
```

Advantages:
- Single source language for all backends
- Single compiler tool (slangc replaces glslang + SPIRV-Cross)
- No runtime transpilation — all compilation is offline
- Structured resource model is native to the language
- Module system and generics solve code reuse and variant explosion

### 1.3 Long-Term Target (R5+)

```
Source:     Slang (.slang) — full use of generics, interfaces, modules
Compile:    slangc (build-time) → final backend format per platform
Variants:   Resolved at build time via Slang specialization
Reflection: Slang reflection API drives automatic resource binding
Binding:    Buffer-based (UBO/push constant/argument buffer), not name-based
```

---

## 2. Resource Binding Model

### 2.1 The Core Problem

OpenGL, Vulkan, and Metal have fundamentally different resource binding models:

| | OpenGL | Vulkan | Metal |
|---|---|---|---|
| Uniforms | `glGetUniformLocation(name)` | Descriptor set + binding (UBO) | Buffer index + offset |
| Per-draw data | Individual `glUniform*` calls | Push constants | `setVertexBytes` / buffer |
| Textures | Sampler unit slot | Descriptor set + binding | Texture index |
| Buffer binding | Named uniform blocks | `VkDescriptorSet` | Argument buffer / `[[buffer(N)]]` |

The current `IShader` interface exposes OpenGL's name-based model:
```cpp
virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
```

This cannot map to Vulkan or Metal without per-call string lookups and translation layers.

### 2.2 Target Abstraction: Slot-Based Binding

The abstraction that works across all three backends is **slot-based buffer and resource binding**:

```cpp
class IShader
{
public:
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual const std::string& GetName() const = 0;

    // --- Buffer Binding (maps to UBO / descriptor / argument buffer) ---
    virtual void BindUniformBuffer(uint32_t slot, const Ref<IBuffer>& buffer) = 0;

    // --- Texture Binding ---
    virtual void BindTexture(uint32_t slot, const Ref<ITexture2D>& texture) = 0;

    // --- Per-Draw Push Data (small, updated every draw call) ---
    virtual void SetPushConstants(ShaderStage stage, const void* data, uint32_t size) = 0;
};
```

How each backend implements this:

| Method | OpenGL | Vulkan | Metal |
|--------|--------|--------|-------|
| `BindUniformBuffer(slot, buf)` | `glBindBufferBase(GL_UNIFORM_BUFFER, slot, ...)` | `vkCmdBindDescriptorSets` | `setVertexBuffer:offset:atIndex:` |
| `BindTexture(slot, tex)` | `glBindTextureUnit(slot, ...)` | Descriptor write | `setFragmentTexture:atIndex:` |
| `SetPushConstants(stage, data, size)` | Upload to a dedicated small UBO or use `glUniform*` | `vkCmdPushConstants` | `setVertexBytes` / `setFragmentBytes` |

### 2.3 Resource Slot Convention

Shader resources are organized by update frequency, following a convention shared across all shaders:

| Slot | Update Frequency | Content | Slang Declaration |
|------|-----------------|---------|-------------------|
| 0 | Per-frame | Camera, lights, time | `ParameterBlock<PerFrameData> gPerFrame;` |
| 1 | Per-material | Albedo, roughness, metallic | `ParameterBlock<MaterialData> gMaterial;` |
| 2 | Per-pass | Pass-specific data (shadow map, etc.) | `ParameterBlock<PerPassData> gPerPass;` |
| Push | Per-draw | Model matrix, normal matrix | `[[vk::push_constant]]` or `ParameterBlock<PerDrawData>` |
| Texture slots | Varies | Albedo map, normal map, shadow map | `Texture2D gAlbedoMap;` |

This convention minimizes descriptor set / buffer rebinding:
- Slot 0 is bound once per frame
- Slot 1 changes per material (batched by material)
- Slot 2 changes per pass
- Push constants change per draw call (cheapest to update)

Slang maps `ParameterBlock<T>` to the appropriate backend mechanism:
- OpenGL: `uniform` block with `layout(binding = N)`
- Vulkan: descriptor set N, binding 0
- Metal: buffer at index N

### 2.4 Evolution Path for IShader

The transition from name-based to slot-based binding happens in phases:

**Phase A (During Slang Migration)**: Keep name-based setters. Slang GLSL output produces
named uniforms within uniform blocks. OpenGL can still access them via
`glGetUniformLocation("PerFrameData.viewProjection")` or by using
`glGetUniformBlockIndex` + `glUniformBlockBinding`.

**Phase B (First Non-OpenGL Backend)**: Add `BindUniformBuffer()` and `BindTexture()` to `IShader`.
OpenGL backend implements them. Material and render passes migrate to buffer-based uploads.
Name-based setters become deprecated but remain for test convenience.

**Phase C (Cleanup)**: Remove name-based setters from `IShader`. All uniform data goes through
pre-packed buffers.

---

## 3. Material System

### 3.1 Architecture Models

Three common approaches, in order of complexity:

**Model A: Material-Centric** — Material owns the Shader.
Used by Unity (SubShader/Pass tags), Unreal (Material Domain + Shading Model).
Too heavy for a lab engine without a mature shader framework.

**Model B: Pass-Centric (Current)** — RenderPass owns the Shader. Material is a pure data container.
Clean and explicit. Best starting point.

**Model C: Hybrid (Target)** — Material describes surface features. Pass defines the rendering task.
The shader is resolved from both. Used by Google Filament, bgfx-based engines.

### 3.2 Current State (Model B)

```
Material
├── Textures (TextureSlot → Ref<ITexture2D>)
├── Properties (string → float/int/vec3/vec4)
└── UploadToShader(const Ref<IShader>& shader)

ForwardPass
├── m_Shader (ForwardLit)
└── Execute() {
        m_Shader->Bind();
        for item:
            item.Material->UploadToShader(m_Shader);
            DrawIndexed();
    }

ShadowPass
├── m_Shader (ShadowDepth)
└── Execute() { /* ignores Material, geometry only */ }
```

Material has no shader reference and no `Bind()` method. It is a pure surface data container.
Passes are fully in control.

### 3.3 Evolution: Model B → Hybrid (Model C)

Each step is triggered by a concrete feature need, not by anticipation.

#### Step 1: ShadingModel Enum

**Trigger**: First non-Lit object (skybox, debug wireframe, emissive-only).

```cpp
enum class ShadingModel : uint8_t { Lit, Unlit };

class Material
{
    ShadingModel m_ShadingModel = ShadingModel::Lit;
public:
    ShadingModel GetShadingModel() const;
};
```

ForwardPass selects shader based on ShadingModel:

```cpp
Ref<IShader> ForwardPass::ChooseShader(const Material& mat) const
{
    return mat.GetShadingModel() == ShadingModel::Unlit ? m_UnlitShader : m_LitShader;
}
```

This is already a simplified Hybrid (C).

#### Step 2: BlendMode Enum

**Trigger**: Transparent or alpha-tested objects.

```cpp
enum class BlendMode : uint8_t { Opaque, Masked, Transparent };
```

Affects render order, shadow pass behavior, and shader variant selection.

#### Step 3: MaterialFeatureFlags + ShaderResolver

**Trigger**: More than 3 shader variants per pass.

```cpp
enum class MaterialFeatureFlag : uint32_t
{
    None      = 0,
    Lit       = 1 << 0,
    AlphaTest = 1 << 1,
    NormalMap = 1 << 2,
    Skinned   = 1 << 3,
};

class ShaderResolver
{
    // Key: (PassType, feature flags hash) → IShader
    Ref<IShader> Resolve(PassType pass, const Material& material) const;
};
```

These named enums are good candidates for shared `magic_enum`-backed helpers once
materials become config/preset/editor data:
- `ShadingModel` / `BlendMode`: straightforward serialization tokens, debug strings,
  and inspector drop-down population.
- `MaterialFeatureFlag`: likely needs a custom flags serializer/UI helper rather than
  raw single-token enum conversion, but still benefits from centralized enum metadata.

Adoption timing: this is a **later wave** after the dependency has already been introduced
and proven out in `InputActionMap` serialization and simpler engine-facing enums such as
renderer/debug/app settings.

User-facing labels should remain an explicit presentation concern. If editor text or
asset tokens must differ from C++ enumerator spellings, add a thin mapping layer on
top of the enum helper rather than exposing raw identifiers directly.

#### Step 4: Slang Interface-Based Variants (Replaces #ifdef Permutations)

**Trigger**: Combinatorial explosion of feature flags.

This is where Slang's language features replace the traditional `#define` permutation system:

```slang
// Define a shading model interface
interface IShadingModel
{
    float3 evaluate(SurfaceData surface, LightData light);
};

// Concrete implementations
struct PBRShading : IShadingModel
{
    float3 evaluate(SurfaceData surface, LightData light) { /* Cook-Torrance */ }
};

struct UnlitShading : IShadingModel
{
    float3 evaluate(SurfaceData surface, LightData light) { return surface.albedo; }
};

// Generic shader — specialized at compile time, zero runtime cost
[shader("fragment")]
float4 forwardLitFS<S : IShadingModel>(VertexOutput vsOut) : SV_Target
{
    S model;
    // ...
    return float4(model.evaluate(surface, light), 1.0);
}
```

The ShaderResolver requests specializations from slangc at build time:

```cmake
# Compile ForwardLit specialized for PBRShading
slangc ForwardLit.slang -target glsl -DSHADING_MODEL=PBRShading -o ForwardLit_PBR.vert.glsl

# Compile ForwardLit specialized for UnlitShading
slangc ForwardLit.slang -target glsl -DSHADING_MODEL=UnlitShading -o ForwardLit_Unlit.vert.glsl
```

Or using Slang's generic specialization API:

```cmake
slangc ForwardLit.slang -target glsl \
    -entry forwardLitFS -type-conformance "S=PBRShading:IShadingModel" \
    -o ForwardLit_PBR.frag.glsl
```

**Advantages over `#ifdef`:**
- Type-safe: compiler checks that all IShadingModel methods are implemented
- IDE-friendly: interfaces have clear contracts, autocompletion works
- Composable: combine multiple interfaces (IShadingModel × ILightModel × IShadowTechnique)
- No dead code: each specialization only includes the code path it uses

### 3.4 Material Data Upload Evolution

**Current**: Per-uniform name-based upload.

```cpp
void Material::UploadToShader(const Ref<IShader>& shader)
{
    shader->SetFloat4("u_Albedo", m_Albedo);
    shader->SetFloat("u_Metallic", m_Metallic);
    // ... one call per property
}
```

**Target**: Buffer-based upload driven by reflection.

```cpp
void Material::UploadToShader(const Ref<IShader>& shader)
{
    // Pack all material properties into a byte buffer matching the shader's
    // MaterialData layout (obtained via Slang reflection at shader load time)
    m_UniformBuffer->SetData(m_PackedData.data(), m_PackedData.size());
    shader->BindUniformBuffer(/*slot=*/1, m_UniformBuffer);

    // Bind textures to their slots
    for (auto& [slot, texture] : m_Textures)
        shader->BindTexture(slot.GetBindingIndex(), texture);
}
```

The packed data layout is built at shader load time from Slang reflection:

```cpp
// At shader creation time
SlangReflection* reflection = slangModule->getLayout();
// Extract MaterialData struct: field names, types, offsets, sizes
// Build a property → byte-offset mapping
// Material uses this mapping to fill m_PackedData when properties change
```

This is detailed in `slang-migration.md` Section 8.

### 3.5 Responsibilities Summary

| Component | Model B (Current) | Step 1-2 (Simple Hybrid) | Step 3-4 (Full Hybrid + Slang) |
|-----------|-------------------|--------------------------|-------------------------------|
| **Material** | Textures + properties | + ShadingModel, BlendMode | + GetFeatureFlags(), packed buffer |
| **RenderPass** | Owns single shader | 2-3 shaders, ChooseShader() | Owns ShaderResolver |
| **ShaderResolver** | N/A | N/A | Resolves (pass, features) → shader |
| **Shader source** | GLSL file, one variant | Multiple .slang files | Single .slang with generics, specialized at build time |
| **Uniform upload** | Name-based setters | Name-based setters | Buffer binding + Slang reflection |

---

## 4. Shader Module Organization

### 4.1 Current (Flat)

```
assets/shaders/
    ForwardLit.vert
    ForwardLit.frag
    ShadowDepth.vert
    ShadowDepth.frag
    TexturePreview.vert
    TexturePreview.frag
```

### 4.2 Target (Modular)

```
assets/shaders/
    ForwardLit.slang              ← main shader (imports modules)
    ShadowDepth.slang
    TexturePreview.slang

    modules/                      ← shared code
        shadow.slang              ← shadow computation (PCF, VSM, etc.)
        lighting.slang            ← BRDF, light evaluation (future)
        common.slang              ← shared structs, utility functions (future)
        noise.slang               ← procedural noise (Phase 6, future)

    interfaces/                   ← variant interfaces (Step 4, future)
        IShadingModel.slang
        ILightModel.slang
        IShadowTechnique.slang

build/shaders/                    ← compiled output (not in repo)
    glsl/
    spirv/
    metal/
```

### 4.3 Module Dependency Graph (Current Scope)

```
ForwardLit.slang ──import──▶ modules/shadow.slang
ShadowDepth.slang           (no imports)
TexturePreview.slang        (no imports)
```

### 4.4 Module Dependency Graph (Future — Phase 4 PBR)

```
ForwardLit.slang ──import──▶ modules/lighting.slang ──import──▶ modules/common.slang
                 ──import──▶ modules/shadow.slang
                 ──import──▶ interfaces/IShadingModel.slang

DeferredShading.slang ──import──▶ modules/lighting.slang
                      ──import──▶ modules/shadow.slang

ShadowDepth.slang    (no imports)
GBuffer.slang        ──import──▶ modules/common.slang
PostProcess.slang    ──import──▶ modules/common.slang
```

---

## 5. Variant Management Strategy

### 5.1 Variant Sources

Shader variants arise from combinations of:

| Axis | Examples | Count |
|------|----------|-------|
| Shading model | Lit, Unlit, Subsurface | 2-4 |
| Blend mode | Opaque, Masked, Transparent | 3 |
| Feature flags | Normal map, Skinned, Double-sided | 2^N |
| Pass type | Forward, Shadow, GBuffer, Depth-prepass | 3-5 |
| Light type | Directional, Point, Spot (future) | 1-3 |

Naive permutation count: 3 × 3 × 8 × 4 × 3 = **864 variants** for a moderately complex engine.

### 5.2 Reduction Strategies (Ordered by Implementation)

**Strategy 1: Separate shaders by pass** (already done).
ShadowDepth doesn't need material variants. TexturePreview is fixed-function.
Only ForwardLit (and future DeferredShading) generate material variants.

**Strategy 2: Slang interfaces** (Step 4 above).
Decouple shading model from the main shader body. Each IShadingModel implementation is
compiled as a separate specialization. This turns the multiplicative explosion into additive:
`N_shading_models + N_light_types` instead of `N × M`.

**Strategy 3: Dynamic branching for low-cost features**.
Features like `useAlbedoMap` (a single texture fetch + branch) do not warrant a separate variant.
Use a runtime boolean in the material UBO. Only create variants for features with significant
shader divergence (different vertex layouts, different output structures).

**Strategy 4: Visibility buffer** (long-term, Phase 7+).
Separate geometry pass from material evaluation entirely. Geometry uses a single universal shader.
Material shading happens in a fullscreen pass that reads visibility data and branches by material type.
This eliminates per-pass × per-material variants for geometry processing.

### 5.3 Variant Compilation

Build-time variant compilation is driven by a shader manifest:

```
# shader_variants.txt (future — not needed until Step 3-4)
ForwardLit : PBRShading      → ForwardLit_PBR
ForwardLit : UnlitShading    → ForwardLit_Unlit
ForwardLit : SubsurfaceShading → ForwardLit_SSS
```

CMake reads this manifest and generates slangc commands for each variant.
The ShaderResolver maps (PassType, MaterialFeatureFlags) → variant name at runtime.

---

## 6. Reflection-Driven Binding

### 6.1 Why Reflection

Without reflection, every change to a shader struct requires manual C++ code updates:

```cpp
// Fragile: if shader adds a field, this struct must be updated manually
struct PerFrameUBO {
    glm::mat4 viewProjection;    // offset 0, size 64
    glm::mat4 lightViewProjection; // offset 64, size 64
    glm::vec3 cameraPosition;    // offset 128, size 12 (16 with padding)
    // ... must match shader layout exactly, including std140 padding
};
```

With reflection, the shader's resource layout is extracted automatically:

```cpp
// At shader load time, query Slang reflection:
// "MaterialData has 3 fields: albedo (float3, offset 0), specularPower (float, offset 12),
//  useAlbedoMap (bool, offset 16)"
//
// Material stores properties in a flat byte buffer and fills it by name→offset mapping.
// No C++ struct needs to mirror the shader layout.
```

### 6.2 Slang vs SPIRV-Cross Reflection

| Aspect | SPIRV-Cross | Slang |
|--------|-------------|-------|
| Input | SPIR-V binary | Slang source or IR |
| Type fidelity | Limited (SPIR-V type system is low-level) | Full (preserves original struct names, generics, interfaces) |
| Binding info | set/binding decorations | Full parameter block layout with semantic meaning |
| Integration | Separate library, runs at runtime | Part of the compiler, available at build time or runtime |

Slang reflection is strictly more capable. It can be used at:
- **Build time**: Generate C++ header structs from shader layouts (codegen)
- **Load time**: Query layouts from Slang IR modules at runtime
- **Design time**: IDE integration, validation, auto-complete

### 6.3 Implementation Plan

**Phase A (Slang migration)**: No reflection. Use hardcoded C++ structs that match shader layouts.
This is the current approach and works fine for 3 shaders.

**Phase B (Buffer binding)**: Introduce Slang runtime reflection to validate that C++ structs match
shader expectations at load time. Log warnings on mismatch.

**Phase C (Full reflection)**: Material properties are dynamically mapped to shader buffer offsets
via reflection. No hardcoded layout structs needed. Adding a property to a shader automatically
makes it available in the Material editor.

---

## 7. Key Principles

1. **Material never owns the final shader program.** It describes surface intent;
   the rendering system decides how to shade it.

2. **Passes remain in control.** Even in the full Hybrid model, each Pass defines
   what task it performs. Only shader selection is delegated to the resolver.

3. **Add complexity only when triggered by a real feature.** An enum with one value
   is noise. A resolver with one branch is overhead. Wait for the second case.

4. **One source of truth per concern.** Shader resource layout is defined in Slang.
   C++ code reads it, never duplicates it (once reflection is in place).

5. **Uniform data flows through buffers, not individual calls.** This is the only
   model that works across OpenGL, Vulkan, and Metal without translation layers.

---

## 8. Timeline Alignment with Project Roadmap

| Project Phase | Shader/Material Milestone |
|--------------|---------------------------|
| Phase 2 (Basic RT) — current | Slang migration (R3b). Keep Model B material. Name-based setters still work. |
| Phase 3 (Modern Pipeline) | ShadingModel enum (Step 1). Buffer-based binding for deferred G-buffer. |
| Phase 4 (PBR) | `modules/lighting.slang` with Cook-Torrance BRDF. MaterialFeatureFlags. |
| Phase 5 (Screen Space) | ShaderResolver (Step 3). Post-process shaders in Slang. |
| Phase 7 (GPU Techniques) | Compute shaders in Slang. Consider visibility buffer. |
| Phase 8 (Ray Tracing) | Slang RT shader support (`[shader("raygeneration")]`, etc.). |

---

## References

- [Slang User Guide](https://shader-slang.org/slang/user-guide/)
- [Slang GitHub](https://github.com/shader-slang/slang)
- [Google Filament Material System](https://google.github.io/filament/Materials.html)
- [Vulkan Descriptor Sets](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap14.html)
- [std140 Layout Rules](https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout)
