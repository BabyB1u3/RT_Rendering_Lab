# Material System — Architecture & Evolution Plan

This document describes the Material system's current design, the target architecture,
and the concrete evolution path between them.

---

## 1. Architecture Models

Three common approaches to Material/Shader/RenderPass ownership in rendering engines:

### Model A: Material-Centric

Material owns the Shader. RenderPass asks Material to bind itself.

```
Material { Shader, Textures, Properties, Bind() }
RenderPass { Execute() { for item: item.Material->Bind(); Draw(); } }
```

**Used by**: Unity (SubShader/Pass tags), Unreal (Material Domain + Shading Model).

**Pros**: Material is self-contained; swapping Material changes rendering behavior.

**Cons**: ShadowPass/DepthPass need different shaders than the Material's surface shader,
requiring a technique/pass-tag system. Material grows into a "god object" managing
parameters, shader references, render state, and variant selection simultaneously.

**Verdict**: Too heavy for a learning/lab engine without a mature shader framework.

### Model B: Pass-Centric

RenderPass owns the Shader. Material is a pure data container.

```
Material { Textures, Properties }
RenderPass { m_Shader; Execute() { m_Shader->Bind(); UploadMaterialData(); Draw(); } }
```

**Pros**: Each pass fully controls its shader — ShadowPass uses depth shader,
ForwardPass uses lit shader. Simple, explicit control flow.

**Cons**: If Material carries no surface semantics (Lit/Unlit, Opaque/Transparent),
the Pass must inspect material data ad-hoc. As passes multiply, material-inspection
code scatters across every pass.

**Verdict**: Best starting point. Clean, explicit, easy to reason about.

### Model C: Hybrid (Target)

Material describes surface features. Pass defines the rendering task.
The final shader is resolved from both.

```
Material { ShadingModel, BlendMode, Textures, Properties }
RenderPass { Execute() {
    shader = ResolveShader(passType, material.features);
    shader->Bind();
    material.UploadToShader(shader);
    Draw();
}}
```

**Used by**: Google Filament, bgfx-based engines.

**Pros**: Correct separation of concerns — Material answers "what surface?",
Pass answers "what task?", ShaderResolver answers "which shader variant?".
Naturally supports alpha test, transparency, unlit objects, skinning, etc.

**Cons**: Requires a shader variant/resolver system. More upfront design.

**Verdict**: The correct long-term target. Implement incrementally as features demand it.

---

## 2. Current State (After Stage 0 Refactor)

The system is a **clean Model B**:

```
Material
├── Textures (TextureSlot → Texture2D)
├── Properties (string → float/int/vec3/vec4)
└── UploadToShader(const Ref<Shader>& shader)   // uploads data to external shader

ForwardPass
├── m_Shader (ForwardLit)
└── Execute() {
        m_Shader->Bind();
        SetGlobalUniforms(...);
        for item:
            item.Material->UploadToShader(m_Shader);
            DrawIndexed();
    }

ShadowPass
├── m_Shader (ShadowDepth)
└── Execute() { /* ignores Material content, only uses geometry */ }
```

Material has **no shader reference** and **no Bind()** method.
It is a pure surface data container. Passes are fully in control.

---

## 3. Evolution Roadmap

Each step is triggered by a **concrete feature need**, not by anticipation.

### Step 1: ShadingModel Enum (Trigger: Skybox or Unlit objects)

When the first non-Lit object appears (skybox, debug wireframe, emissive-only),
add a `ShadingModel` to Material:

```cpp
enum class ShadingModel : uint8_t
{
    Lit,        // Blinn-Phong / PBR — needs lighting uniforms
    Unlit       // Emissive only — no lighting calculation
};

class Material
{
public:
    ShadingModel GetShadingModel() const { return m_ShadingModel; }
    void SetShadingModel(ShadingModel model) { m_ShadingModel = model; }
    // ... existing property/texture API unchanged ...
private:
    ShadingModel m_ShadingModel = ShadingModel::Lit;
};
```

ForwardPass selects shader based on ShadingModel:

```cpp
void ForwardPass::Execute(...)
{
    for (const auto& item : scene.RenderItems)
    {
        Ref<Shader> shader = ChooseShader(*item.Material);
        shader->Bind();
        // set per-object uniforms...
        item.Material->UploadToShader(shader);
        DrawIndexed();
    }
}

Ref<Shader> ForwardPass::ChooseShader(const Material& mat) const
{
    switch (mat.GetShadingModel())
    {
    case ShadingModel::Unlit: return m_UnlitShader;
    case ShadingModel::Lit:   return m_LitShader;
    }
    return m_LitShader;
}
```

ShadowPass also branches — Unlit objects may or may not cast shadows:

```cpp
// ShadowPass can skip Unlit objects, or use the same depth shader.
// Decision made per-project. For a rendering lab, Unlit objects
// probably still cast shadows.
```

**This is already a simplified Hybrid (C).**

### Step 2: BlendMode Enum (Trigger: Transparent objects)

When transparent or alpha-tested objects are needed:

```cpp
enum class BlendMode : uint8_t
{
    Opaque,         // Default — no blending, written to depth buffer
    Masked,         // Alpha test — discard below threshold, still writes depth
    Transparent     // Alpha blend — does not write depth, rendered back-to-front
};

class Material
{
    BlendMode m_BlendMode = BlendMode::Opaque;
    float m_AlphaCutoff = 0.5f;  // only used when Masked
};
```

This affects:
- **Render order**: SceneRenderer sorts Opaque first (front-to-back), then Transparent (back-to-front)
- **ShadowPass**: Masked objects need alpha-test shadow shader; Transparent objects may skip shadows
- **ForwardPass**: Transparent objects use blend-enabled shader variant

```cpp
Ref<Shader> ForwardPass::ChooseShader(const Material& mat) const
{
    if (mat.GetBlendMode() == BlendMode::Transparent)
        return m_TransparentLitShader;
    // Opaque and Masked use the same shader (Masked does discard in fragment)
    return mat.GetShadingModel() == ShadingModel::Unlit ? m_UnlitShader : m_LitShader;
}
```

### Step 3: ShaderResolver (Trigger: >3 shader variants per pass)

When the combination of (ShadingModel × BlendMode × feature flags) grows beyond
what a simple switch can handle, extract the logic into a `ShaderResolver`:

```cpp
class ShaderResolver
{
public:
    // Register shader for a specific (pass, feature) combination
    void Register(PassType pass, MaterialFeatureFlags features, Ref<Shader> shader);

    // Resolve the best shader for the given pass + material
    Ref<Shader> Resolve(PassType pass, const Material& material) const;

private:
    // Key: (PassType, features hash) → Shader
    std::unordered_map<uint64_t, Ref<Shader>> m_ShaderCache;
};
```

Material feature flags are derived from Material state:

```cpp
enum class MaterialFeatureFlag : uint32_t
{
    None        = 0,
    Lit         = 1 << 0,
    AlphaTest   = 1 << 1,
    NormalMap   = 1 << 2,
    Skinned     = 1 << 3,
    DoubleSided = 1 << 4,
};

MaterialFeatureFlags Material::GetFeatureFlags() const
{
    MaterialFeatureFlags flags = MaterialFeatureFlag::None;
    if (m_ShadingModel == ShadingModel::Lit)
        flags |= MaterialFeatureFlag::Lit;
    if (m_BlendMode == BlendMode::Masked)
        flags |= MaterialFeatureFlag::AlphaTest;
    if (GetTexture(TextureSlot::Normal))
        flags |= MaterialFeatureFlag::NormalMap;
    return flags;
}
```

Each pass registers its shader variants at startup:

```cpp
ForwardPass::ForwardPass(...)
{
    m_Resolver.Register(PassType::Forward, Lit,                  LoadShader("ForwardLit"));
    m_Resolver.Register(PassType::Forward, Lit | NormalMap,      LoadShader("ForwardLitNormal"));
    m_Resolver.Register(PassType::Forward, None,                 LoadShader("ForwardUnlit"));
    m_Resolver.Register(PassType::Forward, Lit | AlphaTest,      LoadShader("ForwardLitMasked"));
}

// In Execute():
Ref<Shader> shader = m_Resolver.Resolve(PassType::Forward, *item.Material);
```

### Step 4: Shader Permutation System (Trigger: combinatorial explosion)

If the number of feature combinations grows too large for separate shader files,
move to a **permutation system** with `#define` injection:

```
ForwardLit.glsl (master shader)
  #ifdef HAS_NORMAL_MAP
  #ifdef HAS_ALPHA_TEST
  #ifdef SHADING_MODEL_UNLIT
  ...
```

The ShaderResolver compiles and caches permutations on demand:

```cpp
Ref<Shader> ShaderResolver::Resolve(PassType pass, const Material& mat) const
{
    auto features = mat.GetFeatureFlags();
    auto key = Hash(pass, features);

    if (auto it = m_Cache.find(key); it != m_Cache.end())
        return it->second;

    auto defines = FeaturesToDefines(features);
    auto shader = CompileShaderWithDefines("ForwardLit.glsl", defines);
    m_Cache[key] = shader;
    return shader;
}
```

This is the pattern used by Filament, Unity's shader compiler, and most AAA engines.

---

## 4. Responsibilities at Each Stage

| Component | Model B (Current) | Step 1-2 (Simple Hybrid) | Step 3-4 (Full Hybrid) |
|-----------|-------------------|--------------------------|------------------------|
| **Material** | Textures + properties | + ShadingModel, BlendMode | + GetFeatureFlags() |
| **RenderPass** | Owns single shader, uploads material data | Owns 2-3 shaders, ChooseShader() | Owns ShaderResolver |
| **ShaderResolver** | Does not exist | Does not exist | Resolves (pass, features) → shader |
| **Shader** | GLSL file, loaded once | Multiple GLSL files | Permutation-compiled from master shader |

---

## 5. Key Principles

1. **Material never owns the final shader program.** It describes surface intent;
   the rendering system decides how to shade it.

2. **UploadToShader(shader) is the stable interface.** Regardless of how the shader
   is chosen, Material always uploads its data the same way.

3. **Add complexity only when triggered by a real feature.** An enum with one value
   is noise. A resolver with one branch is overhead. Wait for the second case.

4. **Passes remain in control.** Even in the full Hybrid model, each Pass defines
   what task it performs and delegates only shader selection to the resolver.
   The Pass still sets global uniforms, manages render targets, and controls draw order.
