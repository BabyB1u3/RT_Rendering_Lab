# Shader System Design
## Engine: Slang + C++ Multi-Backend Renderer

This document covers the shader authoring conventions, neutral reflection model, shader program interface, parameter writer, and the Slang compiler subsystem.

The RHI public API that the shader system feeds into is defined in [RHI.md](RHI.md). The boundary between the two systems is `CompiledShaderProgramDesc` — the shader system produces it; `Device::createShaderProgram()` consumes it.

---

## 1. Slang Usage Strategy

## 1.1 Shader authoring style

Shaders should be authored using **logical parameter blocks**.

Recommended pattern:

```hlsl
struct FrameParams
{
    float4x4 viewProj;
    float3 cameraPos;
    float time;
};

struct MaterialParams
{
    float4 baseColor;
    Texture2D albedoTex;
    SamplerState albedoSamp;
};

struct ObjectParams
{
    float4x4 model;
};

ParameterBlock<FrameParams> gFrame;
ParameterBlock<MaterialParams> gMaterial;
ParameterBlock<ObjectParams> gObject;
```

This style gives us a consistent logical interface that Slang can lower differently per backend.

---

## 1.2 Recommended shader parameter grouping

### Set 0: Frame / Scene
Contains:

- camera matrices
- camera position
- frame time
- lighting environment
- shadow maps
- global samplers if needed

### Set 1: Material
Contains:

- material constants
- albedo texture
- normal map
- roughness/metallic maps
- sampler state(s)

### Set 2: Object / Instance
Contains:

- model matrix
- object ID
- skinning palette pointer or per-object structured buffer reference later

### Push constants
Contains:

- tiny frequently changing data
- draw-specific immediate parameters
- debug modes or tiny indices

---

## 1.3 Shader restrictions for first implementation

To keep the architecture stable in version 1, the shader feature subset should be constrained.

### Recommended in v1
- `ParameterBlock<>`
- separate `Texture2D` and `SamplerState`
- simple frame/material/object block structure
- fixed resource counts
- non-bindless resource model
- explicit entry points per stage

### Avoid in v1
- deep nested parameter block hierarchies
- aggressive target-specific special cases
- advanced bindless patterns
- heavy reliance on backend-specific shader quirks
- assuming identical target behavior across Vulkan / Metal

---

## 2. Neutral Engine Shader Reflection Model

The engine defines its own reflection representation independent of Slang runtime APIs.

This prevents backend code from depending directly on Slang data structures and allows cached serialization.

---

## 2.1 Reflection type kinds

`ReflectedTypeKind` describes the **logical type** of a reflected shader field. This is the engine's neutral view of what a field *is*, independent of how it is ultimately bound in any backend. See `ResourceKind` (RHI.md §7.1) for the corresponding RHI binding-level classification.

```cpp
enum class ReflectedTypeKind
{
    Struct,
    ConstantData,
    Texture,
    Sampler,
    Buffer,
    ParameterBlock,
};
```

---

## 2.2 Reflected field representation

`LayoutConvention` describes how constant/struct data is packed in its backing buffer.

**v1 canonical layout policy**: the engine adopts a single canonical CPU-side constant layout for all backends. In v1 this is **`Std430`** (the Slang/SPIR-V default for uniform and storage buffers). The ShaderSystem is responsible for compiling all backend targets with Slang layout decorations that produce a `Std430`-compatible reflection output, or for normalizing the raw Slang reflection output into `Std430` offsets before emitting `ShaderReflectionData`. `ShaderParameterWriter` writes CPU-side constant data assuming this single layout.

Rationale: if layout were truly per-target, the same `"gFrame.viewProj"` field could resolve to different offsets on Vulkan vs. Metal, making the single `ParameterBlockData` blob incorrect for one of them. A canonical layout eliminates this ambiguity at design time. Shader features that cannot be expressed under `Std430` are out of scope for v1.

```cpp
enum class LayoutConvention
{
    Std430,         // canonical v1 engine layout — Slang/SPIR-V default for UBO/SSBO
    Std140,         // legacy UBO layout retained for validation / compatibility tooling
    Scalar,         // tightly packed scalar layout (future; requires Slang scalar layout target)
};
```

`ReflectedField` carries both constant-data layout metadata and resource binding metadata. The two halves are annotated below — only the relevant subset is meaningful for a given `typeKind`.

```cpp
struct ReflectedField
{
    std::string name;
    ReflectedTypeKind typeKind;

    // --- constant / struct layout (relevant when typeKind is ConstantData or Struct) ---
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t alignment = 0;
    uint32_t arrayStride = 0;
    uint32_t matrixStride = 0;
    LayoutConvention layoutConvention = LayoutConvention::Std430;  // always Std430 in v1

    // --- resource binding (relevant when typeKind is Texture, Sampler, Buffer, or ParameterBlock) ---
    uint32_t setIndex = 0;
    uint32_t binding = 0;
    uint32_t arrayCount = 1;
    ShaderStage stageMask = ShaderStage(0);  // ShaderStage defined in §3.2

    std::vector<ReflectedField> children;
};
```

---

## 2.3 Shader reflection root

```cpp
struct PushConstantRangeDesc
{
    ShaderStage stageMask;   // ShaderStage defined in §3.2
    uint32_t    offset;
    uint32_t    size;
};

struct ShaderReflectionData
{
    std::vector<ReflectedField> globals;
    std::vector<PushConstantRangeDesc> pushConstants;
};
```

This representation should be serializable and cacheable.

---

## 3. ShaderProgram

`ShaderProgram` is an RHI object that holds pre-compiled backend shader code and the neutral reflection data extracted from it.

It is created by `Device` from a `CompiledShaderProgramDesc` produced by the **ShaderSystem**. The RHI layer does not invoke the Slang compiler directly — that boundary belongs entirely to the ShaderSystem (see §5).

---

## 3.1 Responsibilities

`ShaderProgram` should own:

- compiled backend code blobs (received from ShaderSystem via `CompiledShaderProgramDesc`)
- stage entry-point metadata
- neutral `ShaderReflectionData` (reflection only — `PipelineLayoutDesc` is derived on demand, not stored)
- backend-specific shader module handles or deferred creation data

---

## 3.2 Interface sketch

```cpp
enum class BackendType
{
    Vulkan,
    Metal,
};

enum class ShaderStage : uint32_t
{
    None     = 0,
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    All      = Vertex | Fragment | Compute,
};

// ---
// ShaderSystem-level descriptors — consumed by the Slang compiler, not by Device directly.
// ---

struct ShaderEntryPointDesc
{
    std::string moduleName;
    std::string entryName;
    ShaderStage stage;
};

// Input to the ShaderSystem compiler (see §5).
struct ShaderSourceDesc
{
    std::vector<ShaderEntryPointDesc> entries;
    std::vector<std::string> defines;
};

// ---
// RHI-level descriptors — output of ShaderSystem, input to Device::createShaderProgram.
// ---

// Distinguishes Metal shader code formats. Ignored for non-Metal backends.
// See RHI_Backend_Metal.md §7 for the full MSL compilation strategy.
enum class MetalCodeFormat
{
    MslSource,  // UTF-8 MSL source text; compiled at runtime via newLibraryWithSource:options:error:
    Metallib,   // pre-compiled .metallib bytes; loaded via newLibraryWithData:error:
};

// One compiled code blob for a specific backend and stage.
// Vulkan:  SPIR-V bytes.
// Metal:   MSL source or .metallib bytes — format indicated by metalCodeFormat.
struct CompiledShaderBlob
{
    BackendType     backend;
    ShaderStage     stage;
    std::vector<uint8_t> code;
    MetalCodeFormat metalCodeFormat = MetalCodeFormat::MslSource;  // Metal only
};

// The complete output package from ShaderSystem compilation.
// This is the only shader-related input the RHI Device needs.
struct CompiledShaderProgramDesc
{
    std::vector<CompiledShaderBlob> blobs;
    ShaderReflectionData reflection;
};

class ShaderProgram
{
public:
    const ShaderReflectionData& getReflection() const;

    // Derives a PipelineLayoutDesc from the stored reflection data.
    // This is a computed result, not a stored member — PipelineLayout does not
    // belong to ShaderProgram. Pass the result to Device::createPipelineLayout()
    // (see RHI.md §7.3).
    PipelineLayoutDesc derivePipelineLayoutDesc() const;

    const void* getBackendCode(BackendType backend, ShaderStage stage) const;
    size_t getBackendCodeSize(BackendType backend, ShaderStage stage) const;
};
```

---

## 4. Shader Parameter Writer

`ShaderParameterWriter` is the **canonical ergonomic interface** for writing shader parameters by name. It resolves field paths against `ShaderReflectionData` and writes into a `ResourceSet`.

`ResourceSet` itself exposes only index-based binding (RHI.md §8.5). All path-based and name-based access goes through `ShaderParameterWriter`.

---

## 4.1 Responsibilities

`ShaderParameterWriter` provides two tiers of access:

#### Convenience tier (path strings)
Resolves a dotted field path at call time against the stored reflection data:
- `"gFrame.viewProj"` → constant offset + size
- `"gMaterial.albedoTex"` → resource binding index

Use this for one-time setup, tool code, and debugging. **Do not use in hot draw loops.**

#### Hot-path tier (pre-resolved handles)
Call `resolveField()` / `resolveBinding()` once at initialization (or after shader hot reload), store the resulting handles, then use the handle overloads on the hot path. This eliminates per-call string lookups.

Handles become invalid after a hot reload; re-resolve after receiving a reload notification.

---

## 4.2 Interface

```cpp
// Pre-resolved handles — obtain once, reuse on the hot path.
// Handles are invalidated by shader hot reload; call resolve*() again after reload.
struct FieldHandle   { uint32_t id = ~0u; bool valid() const { return id != ~0u; } };
struct BindingHandle { uint32_t id = ~0u; bool valid() const { return id != ~0u; } };

class ShaderParameterWriter
{
public:
    explicit ShaderParameterWriter(const ShaderReflectionData& reflection);

    // --- Handle resolution (call once at init / after hot reload) ---
    FieldHandle   resolveField  (std::string_view path) const;
    BindingHandle resolveBinding(std::string_view path) const;

    // --- Hot-path writes (use pre-resolved handles) ---
    void setFloat    (ResourceSet&, FieldHandle,   float value);
    void setFloat4   (ResourceSet&, FieldHandle,   const float4& value);
    void setMatrix4x4(ResourceSet&, FieldHandle,   const float4x4& value);
    void setTexture  (ResourceSet&, BindingHandle, Texture*);
    void setSampler  (ResourceSet&, BindingHandle, Sampler*);
    void setBuffer   (ResourceSet&, BindingHandle, Buffer*, uint64_t offset, uint64_t size);

    // --- Convenience path-based writes (one-time setup / debug only) ---
    void setFloat    (ResourceSet&, std::string_view path, float value);
    void setFloat4   (ResourceSet&, std::string_view path, const float4& value);
    void setMatrix4x4(ResourceSet&, std::string_view path, const float4x4& value);
    void setTexture  (ResourceSet&, std::string_view path, Texture*);
    void setSampler  (ResourceSet&, std::string_view path, Sampler*);
    void setBuffer   (ResourceSet&, std::string_view path, Buffer*, uint64_t offset, uint64_t size);
};
```

---

## 5. Slang Compiler Subsystem

---

## 5.1 Responsibilities

The Slang compiler subsystem should:

- create and own Slang sessions
- load modules
- manage module cache
- compile entry points per backend target
- collect reflection metadata
- produce neutral engine layout structures
- support shader hot reload later

---

## 5.2 Expected outputs

For each shader program, compilation should generate:

- **Vulkan**: SPIR-V bytecode
- **Metal**: MSL source (`MetalCodeFormat::MslSource`) for development builds; pre-compiled `.metallib` bytes (`MetalCodeFormat::Metallib`) for release builds — distinguished by `CompiledShaderBlob::metalCodeFormat`. The `.metallib` bytes are loaded at runtime via `[MTLDevice newLibraryWithData:error:]` (see RHI_Backend_Metal.md §7)
- reflection metadata (populates `ShaderReflectionData`)
- `CompiledShaderProgramDesc` — the complete ready-to-consume RHI input package
- optional serialized cache artifact

`PipelineLayoutDesc` is **not** a direct output of compilation. It is derived on demand by the caller via `ShaderProgram::derivePipelineLayoutDesc()` after the Device has created a `ShaderProgram`.

---

## 5.3 Build pipeline recommendation

A separate shader build step is recommended.

### Inputs
- `.slang` files
- entry point definitions
- macro defines
- target backend
- target profile/capabilities if needed

### Outputs
- `*.spv`
- `*.metal`
- reflection cache data

This enables:

- shader hot reload
- offline inspection
- backend code diffing
- cache reuse
