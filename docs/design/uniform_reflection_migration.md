# Uniform Reflection & Binding Migration Plan

This document defines the concrete migration plan from the current
"Slang source + mixed upload paths + handwritten C++ block layouts"
state to the intended
"slot-based resource binding + reflection-driven buffer packing"
architecture.

It is a companion document to:

- `docs/design/shader_material_system.md`
- `docs/design/metal_backend.md`

Those two documents define the target architecture and design principles.
This document focuses on execution order, repository-local constraints, and
acceptance criteria for each migration step.

---

## 1. Why This Document Exists

The current repository has already moved past the old OpenGL-only shader model:

- shaders are authored in Slang
- build-time compilation to GLSL/MSL exists
- `SetUniformBlock(binding, data, size)` is already used in multiple passes
- Metal backend is implemented and running

However, the repository is still in an intermediate state:

- slot-based upload exists only partially
- shader data is still usually packed by handwritten C++ structs
- reflection metadata is not yet part of the normal build pipeline
- shader source still uses loose `uniform` declarations instead of explicit
  `ParameterBlock<T>` groupings

The macOS `BasicLight` bug made this gap concrete:

- OpenGL path was correct
- Metal path rendered incorrectly
- the shader source was shared
- the binding index was shared
- the bug was entirely caused by the C++ side assuming one backend's layout
  rules for another backend

This plan exists to close that gap without trying to rewrite the renderer in one step.

---

## 2. Current Repository Snapshot

### 2.1 What Is Already True

- `IShader` still exposes name-based setters and `SetUniformBlock()`
- `Material` is still a pass-centric data container with string-keyed properties
- `ForwardPass`, `ShadowPass`, `TexturePreviewPass`, and some tutorial demos already
  use `SetUniformBlock()`
- `Material::UploadToShader()` still exists and still uses name-based setters
- `MetalShader` can load a `.reflect.json` sidecar, but the build does not emit one
- OpenGL and Metal both accept raw bytes for `SetUniformBlock()`

### 2.2 What Is Not True Yet

- there is no shared `PackedUniformBlock` or equivalent helper
- there is no `BindUniformBuffer()` / `SetPushConstants()` API in `IShader`
- there is no reflection-driven validation of pass-owned block layouts
- shaders do not yet organize resources as `PerFrame` / `PerMaterial` / `PerPass`
  explicit blocks
- no part of the runtime treats Slang reflection as the authoritative description
  of buffer packing

### 2.3 Stage Assessment

Using the terminology from `shader_material_system.md`, the repository is currently:

- mostly **Phase A**
- with a few **Phase B-style upload APIs**
- but without the reflection-driven packing that makes Phase B actually portable

That means the next steps should not be:

- "delete name-based setters immediately"
- "rewrite all shaders to `ParameterBlock<T>` at once"
- "generalize materials first"

The next steps should be:

- make reflected layout information available
- consume that information in one shared packer
- migrate high-risk passes first
- only then move the public shader/resource APIs forward

---

## 3. Migration Goals

### 3.1 Primary Goal

Make uniform/block upload backend-agnostic in practice, not just in naming.

Concretely, application/render-pass code should stop depending on handwritten
padding assumptions such as std140 rules or backend-specific "natural" layouts.

### 3.2 Secondary Goals

- keep OpenGL working throughout the migration
- avoid a big-bang shader rewrite
- preserve the pass-centric renderer while its upload path evolves
- make failure modes visible earlier through validation and tests

### 3.3 Non-Goals

This migration does **not** attempt to solve all renderer evolution work:

- no Vulkan runtime implementation in this document
- no material variant system rewrite here
- no full editor-facing material reflection workflow yet
- no immediate removal of all name-based setters

---

## 4. Target End State

The end state of this migration is:

1. Shader resources are grouped by update frequency and bound by stable slots.
2. Slang reflection or generated layout metadata defines the memory layout for each block.
3. C++ packs uniform data through that metadata rather than through handwritten mirror structs.
4. Backends bind buffers/textures; they do not reinterpret application-owned structs.

The target slot convention remains:

- slot 0: `PerFrame`
- slot 1: `PerMaterial`
- slot 2: `PerPass`
- push/per-draw: `PerDraw`

The target upload flow becomes:

```cpp
PackedUniformBlock perFrame(shader->GetUniformBlockLayout(0));
perFrame.Write("ViewProjection", vp);
perFrame.Write("CameraPosition", cameraPos);

PackedUniformBlock perDraw(shader->GetUniformBlockLayout(3));
perDraw.Write("Model", model);
perDraw.Write("NormalMatrix", normalMatrix);

shader->BindUniformBuffer(0, perFrameBuffer);
shader->BindUniformBuffer(3, perDrawBuffer);
shader->BindTexture(1, albedoTexture);
```

The important part is not the exact method spelling. The important part is:

- C++ no longer owns layout rules
- Slang does

---

## 5. Migration Principles

### 5.1 Solve Binding and Packing Separately

Two problems must be treated independently:

- resource binding: which slot / buffer / texture index is used
- buffer packing: where each field lives inside a block

Adding `SetUniformBlock()` solved only the first problem partially.
This plan is about solving the second problem first, then finishing the first properly.

### 5.2 Migrate the Highest-Risk Paths First

The most urgent places are where:

- a block mixes matrices, `float3`, scalars, or bools
- the path already runs on both OpenGL and Metal
- the code uses handwritten layout comments or backend-specific fixups

That means the early priority order is:

1. `BasicLighting`
2. `ForwardPass`
3. `ShadowPass`
4. `TexturePreviewPass`

### 5.3 Keep the Renderer Operational at Every Step

Every phase in this plan must leave the repository in a buildable and runnable state.
If a step cannot be landed incrementally, it should be split.

### 5.4 Prefer One Shared Helper Over Many Smart Call Sites

The plan should not produce "ten carefully hand-maintained call sites."
It should produce one `PackedUniformBlock`-style abstraction used everywhere.

### 5.5 Canonical Field Name Contract

This contract governs how field names are resolved across OpenGL and Metal reflection
paths into `PackedUniformBlock`. It must be satisfied at every phase, including after
Phase 6 restructures shader resources into explicit blocks.

#### The Contract

1. **`PackedUniformBlock` is always block-scoped.**
   One instance represents exactly one uniform block. The block context is established
   at construction time via `GetUniformBlockLayout(binding)`. Within a single block,
   field names are unambiguous by construction — `ShaderUniformBlockLayout::AddField()`
   rejects duplicate names at layout build time.

2. **The field name used in `Write()` / `WriteRequired()` is the Slang original leaf name.**
   Examples: `u_ViewProjection`, `u_CameraPosition`, `u_UseAlbedoMap`.
   This is the name as it appears in the Slang source, without any block variable prefix
   or backend-generated suffix.

3. **Both reflection paths must normalize to the same leaf name.**
   - OpenGL: `NormalizeGLUniformFieldName()` strips the GL block prefix
     (`BlockName.fieldName` → `fieldName`) and Slang-generated numeric suffixes (`_N`).
   - Metal: field names are read directly from the slangc sidecar, which already uses
     the original Slang name.
   The invariant is: after normalization, both backends expose the same key for the
   same logical field.

#### What This Contract Defers

**Block-relative keys** (`PerFrame.ViewProjection`) are not part of this contract.
They would only be needed if a future API allowed writing to multiple blocks through
a single interface. No such API exists today. Introducing block-relative keys now would
require changing every `WriteRequired()` call site with no correctness benefit, since
`PackedUniformBlock` is already block-scoped.

If a cross-block write API is introduced in the future, block-relative keys become
the right choice at that layer. The contract here governs the single-block layer only.

#### Validating the Contract at Phase 6

When Phase 6 introduces `ParameterBlock<T>`, the raw names produced by each backend
will change:

- OpenGL: GL introspection may return `gPerFrame.u_CameraPosition` instead of
  `u_CameraPosition`. The dot-strip in `NormalizeGLUniformFieldName()` handles one
  level of nesting; deeper nesting (e.g. nested structs) requires an update.
- Metal: the sidecar JSON structure changes from a flat `parameters` array to nested
  entries inside a parameter block. `LoadReflectionSidecar()` in `MetalShader.mm`
  must be updated to extract the same leaf name.

Before merging the first Phase 6 shader, verify that both paths produce the same leaf
name for every field in that shader by checking that all `WriteRequired()` calls
succeed on both OpenGL and Metal.

---

## 6. Migration Phases

## 6.1 Phase 0 - Build Metadata Foundation

### Goal

Make reflection metadata part of the normal shader build output.

### Required Changes

1. Update `cmake/CompileShaders.cmake` so Metal shader compilation emits:
   - `<ShaderName>.metal`
   - `<ShaderName>.reflect.json`

2. Decide and document the minimal sidecar schema:
   - shader name
   - stage
   - field name
   - field type / kind
   - offset
   - size
   - optional binding index metadata
   - optional texture base metadata

3. Ensure sidecars are copied into the same compiled shader directory that
   `MetalShader` already scans.

### Repository Files

- `cmake/CompileShaders.cmake`
- possibly top-level asset copy logic in `CMakeLists.txt`

### Exit Criteria

- Building shaders for Metal produces `.reflect.json` files next to `.metal`
- the runtime can find those files without manual copying
- logs clearly indicate whether sidecar loading succeeded or not

### Notes

This phase is small, but it unlocks everything else.
Without it, the runtime has no authoritative layout metadata to consume.

---

## 6.2 Phase 1 - Reflection Consumption and Validation

### Goal

Turn reflection metadata from a dormant capability into an active runtime input.

### Required Changes

1. Strengthen reflection loading in `MetalShader`:
   - keep current sidecar loading
   - log whether sidecar was found, parsed, and applied
   - surface parse/schema errors clearly
   - preserve reflected field types, not just offsets and sizes

2. Introduce a layout representation in the runtime:
   - `ShaderUniformFieldInfo`
   - `ShaderUniformBlockLayout`
   - per-binding field maps

3. Add validation hooks:
   - binding exists
   - field exists
   - write size is valid for field type/size
   - final block size matches reflected block size

Implementation note from the Metal bring-up:

- raw Slang reflection type info is required, not optional metadata
- offset + size alone is not enough to validate legal writes
- example: a reflected `float3` field may occupy 16 bytes even though the logical
  value written from C++ is 12 bytes
- example: a reflected `bool` field may occupy 1 byte in raw reflection, so the
  runtime must not hardcode a 4-byte std140 assumption

### Repository Files

- `src/graphics/backends/metal/MetalShader.mm`
- new runtime layout headers/sources under `src/graphics/` or `src/renderer/`

### Exit Criteria

- reflection data is queryable through a runtime layout object
- loading a shader yields an authoritative layout for at least Metal
- invalid field writes can be detected before draw time

### Notes

This phase does not yet require changing pass code.
It is about making the system able to know the truth before asking call sites to use it.

---

## 6.3 Phase 2 - Introduce a Shared PackedUniformBlock Helper

### Goal

Replace handwritten mirror structs with a reflection-driven block packer.

### Required Changes

1. Add a helper with roughly this shape:

```cpp
class PackedUniformBlock
{
public:
    explicit PackedUniformBlock(const ShaderUniformBlockLayout& layout);

    template<typename T>
    void Write(const std::string& fieldName, const T& value);

    const void* Data() const;
    uint32_t Size() const;
};
```

2. The helper must:
   - allocate the correct block size
   - zero-initialize unused bytes
   - write fields at reflected offsets
   - accept either an exact reflected-size write or a legal logical-size write
     for known reflected field types
   - zero any remaining padding bytes when a logical-size write is smaller than
     the reflected field size
   - reject invalid writes

3. Keep backend concerns out of call sites:
   - pass code asks for a layout
   - pass code writes named fields
   - pass code calls `SetUniformBlock(binding, block.Data(), block.Size())`

### Repository Files

- new helper source/header files
- possibly `IShader` or backend shader classes if layout query methods are added

### Exit Criteria

- at least one real block can be packed without any handwritten mirror struct
- changing a field offset in the shader no longer requires C++ padding edits

### Notes

This is the first phase where the project meaningfully becomes more portable.
In practice, this helper must handle cases like:

- `glm::vec3` written into a reflected `float3` field that occupies 16 bytes
- `bool` written into a reflected field whose size comes from the backend sidecar

If the helper only accepts `sizeof(T) == reflectedSize`, migration will still fail
on legitimate Metal layouts.

---

## 6.4 Phase 3 - Migrate the High-Risk Passes and Demos

### Goal

Remove the most dangerous handwritten block layouts from production paths.

### Migration Order

1. `src/demos/tutorial/06_BasicLighting/BasicLighting.cpp`
2. `src/renderer/passes/ForwardPass.cpp`
3. `src/renderer/passes/ShadowPass.cpp`
4. `src/renderer/passes/TexturePreviewPass.cpp`

### Why This Order

- `BasicLighting` already exposed a cross-backend packing bug
- `ForwardPass` mixes the most fields and has the highest long-term risk
- `ShadowPass` is simple and should become the cleanest example
- `TexturePreviewPass` is small and a good smoke test for trivial blocks

Observed during migration:

- `BasicLighting` on Metal immediately exposed a reflected `float3` field whose
  reflected size was 16 bytes (`u_CameraPosition`)
- `ForwardPass` and `TexturePreviewPass` also require bool writes to follow the
  reflected field size instead of a hardcoded CPU-side encoding assumption

### Required Changes

For each migrated file:

- remove handwritten struct layout assumptions where possible
- construct a `PackedUniformBlock` from shader layout metadata
- write values by field name
- upload via `SetUniformBlock()`

### Exit Criteria

- the four target paths no longer depend on handwritten padding rules
- the temporary Apple-specific hotfix in `BasicLighting.cpp` can be removed
- OpenGL and Metal both render these passes correctly

### Notes

This phase should happen before larger API refactors.
It yields immediate correctness value and reduces pressure on later steps.

---

## 6.5 Phase 4 - Expand Reflection Use Beyond Metal

### Goal

Avoid building a "Metal-only reflection workflow" that later blocks OpenGL/Vulkan alignment.

### Required Changes

1. Decide whether reflection metadata should be:
   - generated for Metal only initially
   - generated per backend
   - generated once from backend-agnostic Slang layout data

2. Prefer a representation that can serve all future backends.

3. Decide how OpenGL consumes the same logical layout:
   - use the same packer and same block bytes
   - bind as UBO via existing `SetUniformBlock()`

### Exit Criteria

- reflected packing is not treated as "Metal workaround code"
- the same packer is used by both OpenGL and Metal call sites

### Notes

This phase is about architecture hygiene.
If skipped, the project risks ending up with:

- OpenGL using one block-packing story
- Metal using another

That would repeat the same problem at a different layer.

### Explicit Non-Goal For The Current Push

Do **not** migrate reflection sidecar parsing onto the shared serialization framework
as part of the current packing migration.

Reasoning:

- the current priority is runtime correctness for reflected packing across OpenGL and Metal
- the reflection sidecar schema is still settling as the packer and loader evolve
- moving the loader to `PropertyTree` too early would add a second axis of churn
  without changing any renderer behavior by itself
- the existing direct JSON parse path is inelegant, but currently local to
  `MetalShader` and therefore contained

Recommended timing:

- earliest reasonable time: after Phase 4 is stable on both backends
- preferred time: after Phase 5 when shader/block APIs have stabilized enough
  that the reflection schema is unlikely to keep changing every few commits
- also acceptable later: alongside Phase 7 / editor-facing material serialization
  work, if the team wants a broader "data loading consistency" pass

What "stable enough" means before focusing this:

- reflected field names, sizes, and types are no longer changing frequently
- the runtime layout objects (`ShaderUniformFieldInfo`, `ShaderUniformBlockLayout`)
  are considered settled
- the loader no longer needs rapid one-off tweaks to match evolving Slang output

Until then, treat serialization integration for reflection sidecars as a deferred
cleanup / consistency task, not a blocker for the reflection migration itself.

---

## 6.6 Phase 5 - Evolve IShader to the Slot-Based Resource API

### Goal

Finish the public resource binding model described in `shader_material_system.md`.

### Required API Changes

Add the following capabilities to `IShader`:

- `BindUniformBuffer(slot, buffer)`
- `BindTexture(slot, texture)`
- `SetPushConstants(stage, data, size)`

The exact final method names may vary, but the semantics should match the design doc.

### Required Backend Changes

OpenGL:

- bind UBOs explicitly by slot
- keep textures as slot-bound resources

Metal:

- move from raw `set*Bytes` uploads toward buffer-backed uniform binding where appropriate
- keep small per-draw data on the cheapest path available

### Exit Criteria

- slot-based resource APIs exist in `IShader`
- at least one real rendering path uses them end-to-end
- name-based setters are no longer required for mainline rendering

### Notes

Do not start here.
This phase becomes safer only after Phase 2 and Phase 3 have removed handwritten layout coupling.

---

## 6.7 Phase 6 - Restructure Shader Resources into Explicit Blocks

### Goal

Move shader source away from loose `uniform` declarations packed into one implicit block.

### Required Changes

Refactor shader resource layout toward:

- `PerFrame`
- `PerMaterial`
- `PerPass`
- `PerDraw`

using explicit blocks or `ParameterBlock<T>` depending on the chosen Slang strategy.

### Recommended Adoption Order

1. `ForwardLit.slang`
2. `ShadowDepth.slang`
3. `TexturePreview.slang`
4. tutorial shaders (`BasicLit`, `UnlitTransformed`, etc.)

### Exit Criteria

- core renderer shaders use explicit resource grouping
- slot assignment becomes clear in shader source
- call sites no longer think in terms of one giant "binding 0 everything block"

### Notes

This phase is intentionally later than packer introduction.
It changes shader source organization and should be done after reflected packing is stable.

### Reflection Path Coupling (Must Read Before Starting)

Phase 6 changes how Slang emits shader resource names, which directly affects both
runtime reflection paths introduced in Phase 1 and Phase 4. These two paths must be
updated together when the first Phase 6 shader lands:

**OpenGL — `NormalizeGLUniformFieldName()` in `ShaderUniformLayout.cpp`**

Currently strips one level of block prefix (`BlockName.fieldName` → `fieldName`).
With `ParameterBlock<T>`, GL introspection may return deeper names such as
`gPerFrame.LightData.direction`. The function handles single-level dot stripping
but not nested structs. Review and extend before migrating the first shader.

**Metal — `LoadReflectionSidecar()` in `MetalShader.mm`**

Currently parses a flat `parameters` array from the slangc sidecar. With
`ParameterBlock<T>`, the sidecar JSON structure will nest uniform fields inside a
parameter block entry rather than listing them at the top level. The parser will
need to recurse into that structure.

**Invariant to maintain:** after both updates, `block.Write("fieldName", value)` must
resolve to the same logical field on both OpenGL and Metal. If the two paths produce
different keys for the same field, `WriteRequired()` will assert in required paths and
plain `Write()` will fail and leave that field zeroed. Verify cross-backend field name
consistency before merging any Phase 6 shader.

---

## 6.8 Phase 7 - Material System Integration

### Goal

Move material uploads from per-property setter calls to packed material buffers.

### Required Changes

1. Keep `Material` as a pass-centric data container.
2. Replace `Material::UploadToShader()` internals with:
   - packed material block generation
   - `BindUniformBuffer(slot 1, materialBuffer)`
   - `BindTexture(slot, texture)`

3. Stop requiring `ForwardPass` to manually pull every material property into a local struct.

### Exit Criteria

- material properties are packed from reflected layout metadata
- textures bind through slot-based APIs
- the main forward path does not need to duplicate material upload logic

### Notes

This is the point where Model B remains, but its upload path becomes much cleaner.

---

## 6.9 Phase 8 - Cleanup and Deprecation

### Goal

Remove transitional mechanisms once the new path is proven.

### Cleanup Targets

- backend-specific hotfix structs
- duplicated block layout comments
- renderer code that manually mirrors shader packing
- most name-based setters from main rendering paths

### Possible Final Deprecations

- keep name-based setters only for:
  - tests
  - tiny demos
  - debug tooling

or remove them entirely if the team decides the API should be strict.

### Exit Criteria

- the main renderer no longer depends on handwritten uniform layouts
- the primary upload path is reflected and slot-based
- new shader fields are added once in Slang and consumed automatically in C++

---

## 7. Recommended Execution Order

If the team wants the highest value with the lowest migration risk, the recommended order is:

1. Phase 0 - emit reflection sidecars
2. Phase 1 - runtime reflection loading and validation
3. Phase 2 - `PackedUniformBlock`
4. Phase 3 - migrate `BasicLighting` + `ForwardPass`
5. Phase 3 - migrate `ShadowPass` + `TexturePreviewPass`
6. Phase 4 - make reflected packing backend-agnostic
7. Phase 5 - evolve `IShader`
8. Phase 6 - explicit shader blocks / `ParameterBlock<T>`
9. Phase 7 - material buffer integration
10. Phase 8 - cleanup and deprecation

If work must be split into milestones, use:

- Milestone A: "No more handwritten block layouts in high-risk passes"
- Milestone B: "Reflection is part of the build and runtime"
- Milestone C: "Slot-based resource API is live"
- Milestone D: "Materials and shaders use explicit block groupings"

---

## 8. File-by-File Impact Map

### Phase 0-1

- `cmake/CompileShaders.cmake`
- `src/graphics/backends/metal/MetalShader.mm`
- possibly asset copy/install logic in `CMakeLists.txt`

### Phase 2

- new shared runtime layout/packing helpers under `src/graphics/` or `src/renderer/`
- possibly `src/graphics/interfaces/IShader.h` if layout query APIs are added

### Phase 3

- `src/demos/tutorial/06_BasicLighting/BasicLighting.cpp`
- `src/renderer/passes/ForwardPass.cpp`
- `src/renderer/passes/ShadowPass.cpp`
- `src/renderer/passes/TexturePreviewPass.cpp`

### Phase 5

- `src/graphics/interfaces/IShader.h`
- `src/graphics/backends/opengl/GLShader.*`
- `src/graphics/backends/metal/MetalShader.*`
- render command / buffer abstractions if new buffer binding helpers are introduced

### Phase 6

- `assets/shaders/ForwardLit.slang`
- `assets/shaders/ShadowDepth.slang`
- `assets/shaders/TexturePreview.slang`
- `assets/shaders/BasicLit.slang`
- possibly shared shader modules under `assets/shaders/modules/`

### Phase 7

- `src/graphics/Material.h`
- `src/graphics/Material.cpp`
- `src/renderer/passes/ForwardPass.cpp`
- future material editor/serialization code if added later

---

## 9. Testing and Validation Strategy

### 9.1 Build-Time Checks

- shader compilation succeeds for target backend
- reflection sidecar exists for every compiled shader
- sidecar schema is valid

### 9.2 Runtime Checks

- shader load logs whether reflection metadata is present
- invalid field writes are caught early
- block sizes match reflected sizes

### 9.3 Render Smoke Tests

Required smoke scenes:

- `06_BasicLighting`
- default `SceneRenderer` forward path
- shadow map preview

These scenes should be checked on:

- OpenGL
- Metal

### 9.4 Regression Focus Areas

- blocks containing mixed `float3` + scalar fields
- bool packing
- matrix packing
- raw reflection type mapping for `float3`, matrices, and bools
- legal logical-size writes into padded reflected fields
- texture binding offsets on Metal

---

## 10. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Reflection sidecar schema changes repeatedly | Loader and build become unstable | Freeze a minimal schema early and version it if needed |
| Reflection loader is migrated to the shared serialization layer too early | Extra churn in both systems while the schema is still moving | Defer serialization integration until reflected packing and shader APIs stabilize |
| Packer is introduced but only used on Metal | Upload path diverges again | Apply the same packer to OpenGL call sites once stable |
| API refactor starts before packer migration | Too many moving parts at once | Finish Phase 2-3 first |
| Shader block refactor happens too early | Large cross-file churn | Delay explicit `ParameterBlock<T>` adoption until the packer is proven |
| Material refactor happens before pass refactor stabilizes | Renderer logic churn spreads too wide | Keep material integration later in the plan |

---

## 11. Definition of Done

This migration is complete when all of the following are true:

- build emits reflection metadata as a normal artifact
- runtime consumes reflection metadata as authoritative layout input
- main rendering passes do not upload handwritten mirror structs
- OpenGL and Metal use the same logical packing path
- `IShader` exposes stable slot-based resource binding APIs
- shader source organizes resources into explicit update-frequency blocks
- materials bind packed buffers/textures rather than issuing per-property setter calls

At that point, the repository can honestly claim:

- one shader source
- one application-side data flow
- multiple backends

without hiding backend-specific layout assumptions in pass code.
