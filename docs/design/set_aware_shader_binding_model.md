# Set-Aware Shader Resource Binding Model

This document defines the next-stage resource binding architecture for RTRLab.
It exists because the repository has now outgrown the transitional flat-slot
model introduced during the OpenGL/Metal uniform-reflection migration.

It is the companion design for:

- `docs/design/shader_material_system.md`
- `docs/design/uniform_reflection_migration.md`
- `docs/design/metal_backend.md`
- `docs/design/set_aware_shader_binding_checklist.md`

Those documents describe the broader renderer and migration context. This one
focuses specifically on the logical resource model, runtime metadata, backend
mapping, and the migration path away from the current slot-only abstraction.

For the implementation-facing task breakdown, read
`set_aware_shader_binding_checklist.md` immediately after this document.

---

## 1. Why This Document Exists

The repository successfully completed the first cross-backend correctness wave:

- shaders are authored in Slang
- OpenGL and Metal both consume reflected uniform-block layouts
- `PackedUniformBlock` removes handwritten packing from the highest-risk passes
- `IShader` now exposes `BindTexture(slot, ...)` and `BindUniformBuffer(slot, ...)`

That work proved the renderer can share shader source and packed bytes across
multiple backends. It also exposed the limit of the current abstraction:

- the engine still speaks in one flat `slot`
- Vulkan and Slang `ParameterBlock<T>` speak in `set + binding`
- Metal reflection reports backend-local resource indices, not the logical
  binding numbers authored in Slang

The first Phase 6 shader experiments made this concrete:

1. `ParameterBlock<T>` emits descriptor-set style bindings in GLSL
   (`layout(set = N, binding = M)`), which does not match the current flat-slot
   engine API.
2. `ConstantBuffer<T>` keeps flat bindings on the GLSL side, but Metal still
   compacts resources to backend-local indices such as `[[buffer(0)]]`,
   `[[buffer(1)]]`, and `[[texture(0)]]`.
3. Therefore, "just extend the Metal parser" is not enough. The runtime also
   needs a logical binding model that is distinct from backend indices.

The conclusion is:

- the flat-slot model was the right bridge for Phase 5
- it is not the right end state for Vulkan-ready shader resource binding

This document defines the replacement.

---

## 2. Design Goals

### 2.1 Primary Goals

1. Represent shader resources in a backend-agnostic logical space.
2. Preserve authored binding intent across OpenGL, Metal, and Vulkan.
3. Allow runtime binding APIs to target logical bindings, not backend-local
   indices.
4. Keep reflection-driven uniform packing intact while the binding model evolves.
5. Unblock future adoption of descriptor-set-style Slang constructs without
   forcing a second engine-wide API rewrite.

### 2.2 Secondary Goals

- Keep current OpenGL and Metal production paths operational during migration.
- Avoid throwing away the already-working Phase 5 APIs all at once.
- Support incremental shader migration rather than a big-bang rewrite.
- Leave room for `ParameterBlock<T>` later without requiring it immediately.

### 2.3 Non-Goals

This document does not define:

- the final material editor/property reflection workflow
- a full Vulkan runtime implementation
- immediate adoption of `SetPushConstants()`
- immediate deletion of flat-slot compatibility helpers

---

## 3. Problem Statement

### 3.1 What the Flat-Slot Model Solved

The current APIs:

```cpp
shader->BindUniformBuffer(slot, buffer);
shader->BindTexture(slot, texture);
```

solved an important problem:

- they removed name-based resource binding from mainline rendering paths
- they gave OpenGL and Metal a common "bind this resource here" vocabulary

That was enough for:

- one implicit global uniform block
- a small number of shared texture slots
- OpenGL and Metal parity for the existing passes

### 3.2 What It Does Not Solve

The flat-slot model cannot express the distinction between:

- logical binding identity authored in shader code
- backend-local binding index chosen by a compiler or backend

Those are not always the same number.

Observed examples:

- GLSL preserved requested bindings such as `binding = 7` and `binding = 9`
- Metal compiled the same resources to compact indices like `buffer(0)`,
  `buffer(1)`, and `texture(0)`

If the engine API only knows about one integer slot, it cannot answer:

- is this `1` the logical binding from Slang, or Metal's compacted buffer index?
- do two resources belong to different sets but happen to share the same backend
  index in different namespaces?
- how should a future Vulkan backend preserve authored descriptor-set layout?

### 3.3 Why This Matters for Vulkan

Vulkan is not the problem by itself. The real issue is that Vulkan makes the
logical resource model explicit:

- `set`
- `binding`
- resource type

An engine abstraction that erases `set` too early forces one of two bad outcomes:

1. Backend-specific patches later to reintroduce it.
2. A second full binding API rewrite once Vulkan or `ParameterBlock<T>` becomes
   unavoidable.

The purpose of this design is to avoid both.

---

## 4. Core Design Decisions

### 4.1 Logical Resource Identity Is `set + binding`

The engine's canonical identifier for a shader resource becomes:

```cpp
struct ShaderBindingPoint
{
    uint32_t Set = 0;
    uint32_t Binding = 0;
};
```

This is the logical binding point authored in shader source and used by engine
code. It is not a backend-local index.

### 4.2 Backend Binding Indices Are Derived Data

Backend-native indices remain necessary, but they are runtime implementation
details derived from compiled shader artifacts.

Examples:

- Vulkan: backend binding is usually identical to logical `{set, binding}`
- OpenGL: backend binding may be a flattened UBO/texture unit index
- Metal: backend binding may be a compacted `bufferIndex`, `textureIndex`, or
  `samplerIndex`

The engine must store both:

- the logical binding point
- the backend-specific mapping required to apply it

### 4.3 Reflection Layouts Stay Block-Scoped

`PackedUniformBlock` and `ShaderUniformBlockLayout` remain valid. Their field-name
contract does not change:

- one `PackedUniformBlock` still corresponds to exactly one block
- `Write()` / `WriteRequired()` still use canonical leaf names
- OpenGL and Metal still normalize to the same leaf names

What does change is how a block is identified:

- today: by one flat binding integer
- target: by one logical `ShaderBindingPoint`

### 4.4 The Current Flat Slots Become a Compatibility Layer

Any bridge-level flat-slot table or helper introduced during Phase 5 should now
be treated as transitional. It describes the current flattened conventions used
by the bridge renderer, not the long-term engine resource model.

In other words:

- `slot 0 = PerFrame`
- `slot 1 = PerPass`
- `slot 2 = PerMaterial`

remain useful for the current bridge, but they are no longer the target
architecture described by the design docs.

---

## 5. Target Public API

### 5.1 Binding Types

Recommended foundational types:

```cpp
enum class ShaderResourceKind : uint8_t
{
    UniformBuffer,
    Texture2D,
    Sampler,
    CombinedTextureSampler,
    StorageBuffer,
};

struct ShaderBindingPoint
{
    uint32_t Set = 0;
    uint32_t Binding = 0;

    bool operator==(const ShaderBindingPoint&) const = default;
};
```

Optional but recommended helpers:

```cpp
namespace ShaderBindingSets
{
    inline constexpr uint32_t FramePass = 0;
    inline constexpr uint32_t Material = 1;
    inline constexpr uint32_t Draw = 2;
}
```

And engine-level convenience constants:

```cpp
namespace ShaderBindingPoints
{
    inline constexpr ShaderBindingPoint PerFrame   { 0, 0 };
    inline constexpr ShaderBindingPoint PerPass    { 0, 1 };
    inline constexpr ShaderBindingPoint PerMaterial{ 1, 0 };
    inline constexpr ShaderBindingPoint PerDraw    { 2, 0 };
}
```

These names are illustrative. Exact naming can be refined during implementation,
but the logical model should remain `set + binding`.

### 5.2 `IShader` Direction

Target `IShader` shape:

```cpp
class IShader
{
public:
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    virtual void BindUniformBuffer(
        ShaderBindingPoint binding,
        const Ref<IUniformBuffer>& buffer) = 0;

    virtual void BindTexture(
        ShaderBindingPoint binding,
        const Ref<ITexture2D>& texture) = 0;

    virtual const ShaderUniformBlockLayout* GetUniformBlockLayout(
        ShaderBindingPoint binding) const = 0;
};
```

Notes:

- `SetPushConstants()` stays out of this first redesign wave.
- Existing slot-based overloads may remain temporarily as compatibility shims for
  `{set = 0}`-style bindings or for tutorial/demo code.
- The runtime should stop using raw integers as the primary public identity for a
  resource binding once the new API lands.

### 5.3 Why No Stage Parameter on `BindTexture()`

The current design keeps `BindTexture()` stage-agnostic even in the set-aware
model.

Reasoning:

- logical binding identity is orthogonal to backend stage plumbing
- OpenGL texture units are already stage-agnostic
- Metal can bind to both vertex and fragment stages internally
- Vulkan stage visibility belongs in layout metadata and descriptor declarations,
  not in the high-level call site for today's renderer usage

If a future use case requires stage-selective texture binding, add it as an
extended API rather than baking it into the first set-aware surface.

---

## 6. Runtime Metadata Model

### 6.1 Resource-Level Metadata

The shader runtime needs a richer description than "binding N has this block size."

Recommended shape:

```cpp
struct ShaderResourceLayout
{
    std::string Name;
    ShaderResourceKind Kind;
    ShaderBindingPoint LogicalBinding;
    ShaderStageFlags Visibility;
};
```

For uniform-buffer-like resources, extend with block layout data:

```cpp
struct ShaderUniformBlockLayout
{
    std::string Name;
    ShaderBindingPoint LogicalBinding;
    uint32_t Size = 0;
    // existing field map / field metadata remains here
};
```

### 6.2 Backend Binding Metadata

Each backend also needs a translation layer from logical binding identity to the
actual indices used by the compiled artifact.

Recommended shape:

```cpp
struct ShaderBackendBinding
{
    GraphicsAPI Backend;
    ShaderResourceKind Kind;
    ShaderBindingPoint LogicalBinding;

    std::optional<uint32_t> BufferIndex;
    std::optional<uint32_t> TextureIndex;
    std::optional<uint32_t> SamplerIndex;
};
```

For Metal, this is where compacted backend indices live.
For OpenGL, this may hold flattened binding/unit numbers.
For Vulkan, it may simply mirror the logical binding.

### 6.3 Source of Truth

The runtime should consume two conceptually distinct layers of information:

1. **Logical resource layout**
   - authored resource name
   - logical `{set, binding}`
   - resource kind
   - block field layout for uniform buffers

2. **Backend binding map**
   - backend-local indices required to actually bind the compiled artifact

Whether these are emitted as:

- one richer reflection sidecar
- two sidecars
- one build-generated data blob

is an implementation choice. The important design requirement is that the engine
must preserve both layers explicitly.

### 6.4 Field-Name Contract Remains Unchanged

The canonical field-name contract defined during the reflection migration remains
the same:

- block-scoped
- leaf-name based
- backend-normalized to the same key

This design intentionally separates:

- resource binding identity
- field lookup inside a reflected block

Those concerns should not be conflated.

---

## 7. Backend Mapping Strategy

### 7.1 Vulkan

Vulkan is the easiest backend under this model.

- logical binding is already `set + binding`
- descriptor set layouts map directly
- push constants remain a separate later feature

This is the primary reason to adopt the logical model now rather than later.

### 7.2 Metal

Metal is the backend that most strongly justifies this redesign.

Observed behavior:

- Slang may compact resources to backend-local `[[buffer(N)]]` and
  `[[texture(N)]]` indices
- these backend indices do not necessarily equal the authored logical bindings

Therefore the Metal runtime must:

1. load logical binding metadata
2. load backend-local indices from the compiled Metal reflection
3. build a logical-to-backend binding map
4. use that map inside `BindUniformBuffer()` / `BindTexture()`

This is the core architectural fix. The engine should no longer assume that the
number passed in from gameplay/render code is the same number Metal uses.

### 7.3 OpenGL

OpenGL does not naturally expose descriptor-set semantics. The recommended design
therefore treats OpenGL as a backend that consumes a lowered representation.

Recommended implementation strategy:

- keep logical `{set, binding}` in engine/runtime metadata
- compile or lower the GLSL/OpenGL artifact to flat binding/unit indices
- store the resulting OpenGL-specific indices in backend binding metadata

That gives OpenGL a stable runtime path without forcing the rest of the engine to
erase `set`.

This is preferable to making the engine's public API permanently look like OpenGL.

---

## 8. Shader Authoring Model

### 8.1 Immediate Recommendation

After this redesign starts, the first explicit-block migrations should prefer:

- `ConstantBuffer<T>` for uniform-buffer-style data
- explicit binding annotations

Reason:

- it is a smaller step from the current implicit-block shaders
- it avoids immediately requiring full resource-group semantics everywhere
- it still benefits from the set-aware engine model

### 8.2 `ParameterBlock<T>` Status

`ParameterBlock<T>` is no longer rejected as a language feature.
Instead, it is classified as:

- **supported by the target architecture**
- **not required for the first production migration wave**

That distinction matters.

The old flat-slot engine could not cleanly host `ParameterBlock<T>` as a final
resource model. The new set-aware design is explicitly intended to remove that
limitation. Even so, the first practical migrations may still prefer explicit
constant buffers until the runtime, metadata, and test coverage are fully stable.

### 8.3 Recommended Logical Set Convention

The following convention is recommended for the first set-aware design wave:

| Set | Purpose | Typical Resources |
|-----|---------|-------------------|
| 0 | Frame / pass-scoped resources | `PerFrame`, `PerPass`, pass-global textures |
| 1 | Material-scoped resources | `PerMaterial`, albedo/normal/roughness textures |
| 2 | Draw / instance-scoped resources | `PerDraw`, skinning/instance data |

Within each set, bindings are assigned explicitly per resource.

Example:

| Logical Binding | Meaning |
|-----------------|---------|
| `{0, 0}` | `PerFrame` |
| `{0, 1}` | `PerPass` |
| `{0, 8}` | pass/global shadow map |
| `{1, 0}` | `PerMaterial` |
| `{1, 1}` | material albedo texture |
| `{1, 2}` | material normal texture |
| `{2, 0}` | `PerDraw` |

The specific numbers can evolve, but the important rule is that:

- sets capture update-frequency domains
- bindings identify resources within a set

---

## 9. Migration Plan

### 9.1 Stage 1 - Introduce Logical Binding Types

Add:

- `ShaderBindingPoint`
- `ShaderResourceKind`
- hash/comparison helpers as needed

Then extend runtime layout objects so blocks/resources are keyed by logical
binding, not just flat slot.

### 9.2 Stage 2 - Expand Reflection / Build Metadata

Update shader build outputs and runtime parsing so each shader carries:

- logical resource bindings
- backend-local binding indices
- reflected block layout data

This is the stage where Metal's compacted indices stop being a blocker, because
they become just one backend view layered under a preserved logical binding.

### 9.3 Stage 3 - Add Set-Aware `IShader` APIs

Introduce set-aware binding methods and keep flat-slot compatibility overloads
temporarily where useful.

Migration rule:

- new renderer code uses `ShaderBindingPoint`
- old tutorial/demo code may continue to use flat slots during the bridge period

### 9.4 Stage 4 - Pilot Shader Migration

Once runtime metadata and APIs are in place:

1. migrate `TexturePreview` first
2. migrate `ShadowDepth`
3. migrate `ForwardLit`

The pilot success criteria are:

- logical bindings are stable across OpenGL and Metal
- `PackedUniformBlock` still resolves the same canonical field names
- no backend-specific binding numbers leak into pass code

### 9.5 Stage 5 - Material Integration

Once the resource model is stable:

- move material upload to logical bindings
- group material buffer + textures into a coherent set
- stop teaching passes to know material texture unit details directly

### 9.6 Stage 6 - Remove Transitional Flat-Slot Assumptions

Only after the set-aware path is proven:

- retire bridge-level flat-slot helpers as a primary design concept
- narrow or remove flat-slot overloads
- update tests to target logical binding points directly

---

## 10. Why Not Simpler Alternatives

### 10.1 "Keep Flat Slots and Patch Metal"

Rejected because:

- it still erases `set`
- it forces backend-specific recovery logic later
- it does not provide a clean Vulkan-facing model

### 10.2 "Just Add a Binding Manifest"

A logical binding manifest is useful, but by itself it only solves metadata.
It does not solve the engine API problem if the public abstraction remains a flat
slot integer.

The engine still needs a logical binding type in its runtime API.

### 10.3 "Jump Directly to `ParameterBlock<T>` Everywhere"

Rejected as the first implementation step because it combines too many moving
pieces at once:

- explicit block migration
- new logical binding model
- backend parser changes
- potential resource-group semantics

The new architecture should allow `ParameterBlock<T>`, but the migration should
still proceed incrementally.

---

## 11. Definition of Done

This redesign is complete when all of the following are true:

- `IShader` and runtime layout APIs are keyed by logical `set + binding`
- shader metadata preserves both logical bindings and backend-local indices
- OpenGL, Metal, and future Vulkan backends bind resources through the same
  logical API
- `PackedUniformBlock` continues to operate on canonical reflected block layouts
- production shaders can move to explicit resource groupings without backend
  binding numbers leaking into pass code

At that point, the engine will finally have:

- one logical shader resource model
- multiple backend-specific binding implementations

instead of asking gameplay/render code to pretend those are the same thing.
