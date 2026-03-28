# Shader & Material System - Material-Focused Design Plan

This document now keeps only the material-system design and the parts of shader
selection/upload that are directly about material architecture.

The following topics are intentionally owned by other documents and are not
repeated here:

- logical binding-model cleanup and compatibility-layer removal
- reflection/layout invariants for portable packing
- backend-specific remaining work, especially Metal

See:

- `shader_binding.md`
- `metal_backend.md`

---

## 1. Material System

### 1.1 Architecture Models

Three common approaches, in order of complexity:

**Model A: Material-Centric** - Material owns the Shader.
Used by Unity (SubShader/Pass tags), Unreal (Material Domain + Shading Model).
Too heavy for a lab engine without a mature shader framework.

**Model B: Pass-Centric (Current)** - RenderPass owns the Shader. Material is a pure data container.
Clean and explicit. Best starting point.

**Model C: Hybrid (Target)** - Material describes surface features. Pass defines the rendering task.
The shader is resolved from both. Used by Google Filament, bgfx-based engines.

### 1.2 Current State (Model B)

```
Material
  |- Textures (TextureSlot -> Ref<ITexture2D>)
  |- Properties (string -> float/int/vec3/vec4)
  `- UploadToShader(const Ref<IShader>& shader)   // legacy compatibility path

ForwardPass
  |- m_Shader (ForwardLit)
  `- Execute() {
         m_Shader->Bind();
         Bind shadow texture;
         for item:
             read material properties directly;
             pack reflected block with pass/draw/material fields;
             bind albedo texture by TextureSlot;
             DrawIndexed();
     }

ShadowPass
  |- m_Shader (ShadowDepth)
  `- Execute() { /* geometry only */ }
```

Material has no shader reference and no `Bind()` method. It is still a pure
surface data container. Passes remain fully in control.

Current repository note:

- This pass-centric ownership model is implemented.
- The maintained upload path is now mixed rather than purely name-based:
  - `Material::UploadToShader()` still exists and uses name-based setters
  - `ForwardPass`, `ShadowPass`, and `TexturePreviewPass` already use reflected
    layouts plus `PackedUniformBlock`
  - `ForwardPass` still reads material values directly and binds the albedo
    texture itself, which means the material side has not yet reached the same
    steady-state upload model as the pass side

Material-specific work that still remains:

- reduce or remove `TextureSlot` as a public GPU binding contract
- move material-owned scalar/vector data to a clear reflected per-material block
- stop relying on pass-local extraction of material fields as the long-term
  upload path
- align material-owned textures with the final logical resource ownership model

So the repository is firmly in Model B, but the material/resource boundary is
still transitional.

### 1.3 Evolution: Model B -> Hybrid (Model C)

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
    // Key: (PassType, feature flags hash) -> IShader
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

// Generic shader - specialized at compile time, zero runtime cost
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
- Composable: combine multiple interfaces (IShadingModel x ILightModel x IShadowTechnique)
- No dead code: each specialization only includes the code path it uses

### 1.4 Material Data Upload Evolution

The repository currently lives between the two ends shown below: the legacy
per-uniform material upload path still exists, while the maintained pass path
has already moved to reflected packing.

**Legacy baseline**: Per-uniform name-based upload.

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
    shader->BindUniformBuffer(/*binding=*/{1, 0}, m_UniformBuffer);

    // Bind textures to their logical material bindings
    for (auto& [binding, texture] : m_Textures)
        shader->BindTexture(binding, texture);
}
```

The packed data layout is built at shader load time from Slang reflection:

```cpp
// At shader creation time
SlangReflection* reflection = slangModule->getLayout();
// Extract MaterialData struct: field names, types, offsets, sizes
// Build a property -> byte-offset mapping
// Material uses this mapping to fill m_PackedData when properties change
```

The same idea should be applied to pass-owned uniform data as well, not only
material-owned data. In other words, `MaterialData`, `PerFrameData`, `PerPassData`,
and temporary demo-specific blocks should all be packed from the same reflected
layout source. Otherwise the engine ends up with two systems:

- materials packed correctly from reflection
- passes/demos still relying on fragile handwritten layout structs

That split would reintroduce platform-specific bugs in exactly the places where
`SetUniformBlock()` is supposed to make uploads portable.

Recommended helper shape:

```cpp
PackedUniformBlock block(shader->GetUniformBlockLayout(/*binding=*/0));
block.Write("u_ViewProjection", viewProjection);
block.Write("u_Model", model);
block.Write("u_LightColor", lightColor);
block.Write("u_LightIntensity", lightIntensity);
shader->SetUniformBlock(0, block.Data(), block.Size());
```

Whether this helper is backed by runtime reflection, build-time JSON sidecars,
or generated C++ layout code is an implementation choice. The important property
is that the pass no longer duplicates shader layout rules manually.

### 1.5 Responsibilities Summary

| Component | Model B (Current) | Step 1-2 (Simple Hybrid) | Step 3-4 (Full Hybrid + Slang) |
|-----------|-------------------|--------------------------|-------------------------------|
| **Material** | Textures + properties | + ShadingModel, BlendMode | + GetFeatureFlags(), packed buffer |
| **RenderPass** | Owns single shader | 2-3 shaders, ChooseShader() | Owns ShaderResolver |
| **ShaderResolver** | N/A | N/A | Resolves (pass, features) -> shader |
| **Shader source** | Slang file, one variant | Multiple .slang files | Single .slang with generics, specialized at build time |
| **Uniform upload** | Mixed transitional path | Name-based setters or reflected packing, depending on stage | Buffer binding + Slang reflection |

---

## 2. Variant Management Strategy

### 2.1 Variant Sources

Shader variants arise from combinations of:

| Axis | Examples | Count |
|------|----------|-------|
| Shading model | Lit, Unlit, Subsurface | 2-4 |
| Blend mode | Opaque, Masked, Transparent | 3 |
| Feature flags | Normal map, Skinned, Double-sided | 2^N |
| Pass type | Forward, Shadow, GBuffer, Depth-prepass | 3-5 |
| Light type | Directional, Point, Spot (future) | 1-3 |

Naive permutation count: 3 x 3 x 8 x 4 x 3 = **864 variants** for a moderately complex engine.

### 2.2 Reduction Strategies (Ordered by Implementation)

**Strategy 1: Separate shaders by pass.**
ShadowDepth does not need material variants. TexturePreview is fixed-function.
Only ForwardLit (and future DeferredShading) should generate material variants.

**Strategy 2: Slang interfaces** (Step 4 above).
Decouple shading model from the main shader body. Each `IShadingModel`
implementation is compiled as a separate specialization. This turns the
multiplicative explosion into additive:
`N_shading_models + N_light_types` instead of `N x M`.

**Strategy 3: Dynamic branching for low-cost features.**
Features like `useAlbedoMap` (a single texture fetch + branch) do not warrant a
separate variant. Use a runtime boolean in the material UBO. Only create
variants for features with significant shader divergence (different vertex
layouts, different output structures).

**Strategy 4: Visibility buffer** (long-term).
Separate geometry pass from material evaluation entirely. Geometry uses a single
universal shader. Material shading happens in a fullscreen pass that reads
visibility data and branches by material type. This eliminates per-pass x
per-material variants for geometry processing.

### 2.3 Variant Compilation

Build-time variant compilation is driven by a shader manifest:

```
# shader_variants.txt (future - not needed until Step 3-4)
ForwardLit : PBRShading         -> ForwardLit_PBR
ForwardLit : UnlitShading       -> ForwardLit_Unlit
ForwardLit : SubsurfaceShading  -> ForwardLit_SSS
```

CMake reads this manifest and generates slangc commands for each variant.
The ShaderResolver maps `(PassType, MaterialFeatureFlags)` to variant name at
runtime.

---

## 3. Material Design Principles

1. **Material never owns the final shader program.** It describes surface intent;
   the rendering system decides how to shade it.

2. **Passes remain in control.** Even in the full Hybrid model, each Pass defines
   what task it performs. Only shader selection is delegated to the resolver.

3. **Add complexity only when triggered by a real feature.** An enum with one value
   is noise. A resolver with one branch is overhead. Wait for the second case.

4. **Material-facing data should ultimately flow through reflected buffer layouts,**
   not through permanently handwritten uniform call sequences.

---

## References

- [Slang User Guide](https://shader-slang.org/slang/user-guide/)
- [Slang GitHub](https://github.com/shader-slang/slang)
- [Google Filament Material System](https://google.github.io/filament/Materials.html)
