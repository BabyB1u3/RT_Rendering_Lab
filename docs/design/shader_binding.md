# Set-Aware Shader Binding - Remaining Work Plan

This document is now the single design tracker for the set-aware shader binding
work in RTRLab.

The earlier split between a "model" document and a separate implementation
checklist is no longer useful for the repository's current state. The
foundational migration has already landed in code, so this file now tracks only
the work that still remains.

**Related documents**:

- `docs/design/shader_system.md`
- `docs/design/material_system.md`
- `docs/design/metal_backend.md`
- `docs/design/testing_strategy.md`

---

## 1. Scope

### 1.1 What This Document Covers

- the unfinished work after the first set-aware runtime migration landed
- the remaining path from the current bridge-era binding model to the intended
  logical multi-set resource model
- cleanup of compatibility layers that are still keeping older renderer/demo
  code alive

### 1.2 What This Document Intentionally Omits

The following are already in the repository and are not restated here as active
work items:

- `ShaderBindingPoint`, `ShaderResourceKind`, and hash/equality support
- logical `{ set, binding }` identity on `ShaderUniformBlockLayout`
- set-aware `IShader` overloads for:
  - `BindUniformBuffer(ShaderBindingPoint, ...)`
  - `BindTexture(ShaderBindingPoint, ...)`
  - `GetUniformBlockLayout(ShaderBindingPoint)`
- runtime metadata structures for logical resource layout and backend binding
  metadata
- backend binding resolution through preserved metadata in OpenGL and Metal
- end-to-end pilot migrations for:
  - `TexturePreview`
  - `ShadowDepth`
  - `ForwardLit`

### 1.3 Non-Goals

- Vulkan runtime implementation
- full material editor/property reflection workflow
- backend-specific Metal bring-up details
- re-documenting already-completed migration batches

---

## 2. Current Gap Summary

The repository has already crossed the important architectural threshold:

- public runtime APIs understand logical binding points
- backend execution can translate logical bindings to backend-local indices
- the production pilot shaders no longer prove the model conceptually only;
  they already run through it

The remaining gap is not "make set-aware binding exist." The remaining gap is:

- make the final logical multi-set model the normal production path
- stop relying on bridge-era flat `{0, N}` conventions outside temporary
  compatibility code
- finish the broader renderer/material adoption that the pilot wave did not try
  to solve

---

## 3. Steady-State Binding and Reflection Invariants

Some parts of the earlier migration are no longer active work items, but they
are still important architectural constraints that this document now owns.

### 3.1 Reflected Layouts Remain Authoritative

The repository should continue to treat reflected metadata, not handwritten C++
mirror structs, as the source of truth for portable shader data layout.

That applies to:

- `ShaderUniformBlockLayout`
- `ShaderResourceLayout`
- `ShaderBackendBinding`
- `PackedUniformBlock`

The logical binding model and the reflection-driven packing model are separate
concerns, but they must continue to work together:

- binding metadata says which logical resource is being targeted
- reflected layout metadata says how bytes are written into that resource

### 3.2 Field-Name Contract Must Stay Stable

`PackedUniformBlock::Write("fieldName", value)` must continue to target the same
logical field across OpenGL and Metal.

That means:

- OpenGL reflection names must normalize to a canonical leaf field name
- Metal reflection sidecar names must match that same canonical leaf field name
- call sites should write logical leaf names, not backend-decorated names

The implementation anchor for this contract is:

- `NormalizeGLUniformFieldName()`

This contract exists so backend/compiler differences such as:

- block prefixes
- array suffixes
- temporary numeric suffixes

do not leak into gameplay/render code as part of the field lookup key.

### 3.3 Packed Writes Must Preserve Reflected Type Semantics

`PackedUniformBlock` should continue to treat reflected offset, size, and field
type as authoritative.

This is the protection against cross-backend layout bugs such as:

- logical `float3` writes into padded 16-byte reflected fields
- backend-specific bool size/alignment differences
- code paths silently assuming a handwritten struct layout is portable

The binding cleanup described later in this document must not regress these
packing guarantees.

---

## 4. Remaining Goals

### 4.1 Finish the Final Logical Resource Layout

The current runtime speaks in `ShaderBindingPoint`, but the repository still
mostly behaves as if the working layout were a flattened set-0 bridge.

Current evidence of this transitional state:

- `ForwardLit` still keeps its active resources in the current set-0 layout
- `ForwardPass` explicitly says it is keeping the current flat set-0 binding
  layout while moving to logical binding points
- engine constants for `FramePass`, `Material`, and `Draw` exist, but they are
  not yet the real production organization of shader resources

Target state:

- set 0 contains frame/pass-scoped resources
- set 1 contains material-scoped resources
- set 2 contains draw/instance-scoped resources
- renderer code binds by logical ownership domain, not by compatibility slot

Required work:

- move production shader/resource layouts from the current flat set-0 pattern to
  the intended multi-set organization
- define the concrete resource split for frame/pass, material, and draw domains
- update production shader source so the authored bindings reflect that split
- update renderer code so the binding points used at call sites match the final
  layout instead of the bridge layout

Acceptance criteria:

- `ShaderBindingSets::FramePass`, `ShaderBindingSets::Material`, and
  `ShaderBindingSets::Draw` are used by real renderer code as the actual
  resource layout, not just as future-facing constants
- `ForwardLit` no longer treats material textures/resources as part of the
  bridge-era set-0 layout

### 4.2 Complete the Material-Side Adoption

The pilot migration intentionally stopped short of the full material rewrite.
That boundary still remains.

Current evidence:

- `Material` still exposes textures through `TextureSlot`
- `Material` still carries a legacy `UploadToShader()` path
- `ForwardPass` still manually extracts material values and binds material
  textures itself

This means the repository has set-aware shader binding, but not yet a final
set-aware material/resource organization.

Required work:

- reduce or remove `TextureSlot` as a de facto GPU binding contract
- express material-owned resources through the same logical binding model as the
  rest of the renderer
- decide which material data belongs in per-material buffers versus per-draw
  buffers
- align `Material`, `ForwardPass`, and shader resource layout around the same
  ownership model

Acceptance criteria:

- material resources are organized by logical binding ownership rather than by
  legacy texture-unit assumptions
- production material paths no longer require passes to manually preserve a
  bridge-era texture binding convention

### 4.3 Finish Metadata Coverage and Schema Refinement

The current metadata path is sufficient for the live binding path, but it is
not yet clearly the final production shape for every resource kind and backend
case.

Current evidence:

- OpenGL still preserves some resource bindings by parsing generated GLSL
- Metal still carries fallback behavior that can collapse to flat bindings when
  richer metadata is absent
- the active shader/resource set in the repository is still small enough that
  metadata coverage has only been proven on a limited production surface

Required work:

- decide the stable runtime/build metadata shape for logical resource layout and
  backend binding metadata
- ensure the metadata model covers the resource kinds the renderer intends to
  support, not just the pilot cases already in use
- make backend fallback-to-flat behavior a temporary compatibility path rather
  than part of the assumed normal runtime contract

Acceptance criteria:

- the repository has one clearly documented metadata contract for:
  - logical resource identity
  - backend-local binding translation
  - reflected block layout where applicable
- production binding behavior does not depend on "best effort" compatibility
  fallback when proper metadata should exist

### 4.4 Migrate Remaining Bridge-Era Call Sites

The foundational API expansion is done, but wider adoption is not.

Current evidence:

- deprecated flat-slot overloads still exist on `IShader`
- `MakeFlatShaderBindingPoint()` still exists as an active bridge helper
- some tutorials/demos still use flat assumptions or raw `SetUniformBlock()`
  uploads as their normal path

Required work:

- migrate remaining renderer/demo/tutorial code to explicit logical binding
  points where that code is expected to remain part of the maintained path
- stop adding new code that relies on flat-slot bridge helpers
- narrow the compatibility surface once production and maintained demo code no
  longer depends on it

Examples of remaining transitional code paths:

- raw `SetUniformBlock(binding, data, size)` usage in older tutorial/demo paths
- bridge-era flat binding assumptions in shader source and pass code outside the
  already-migrated pilots

Acceptance criteria:

- maintained renderer and demo code bind through explicit `ShaderBindingPoint`
- flat-slot helpers exist only as clearly temporary compatibility support, or
  are removed entirely

### 4.5 Re-Evaluate the Public API Boundary After Cleanup

Some API decisions were intentionally deferred while the migration was still in
flight.

The main unresolved boundary question that still belongs here is:

- whether the cross-backend shader API still needs a future push-constant style
  concept, or whether the final material/draw binding model makes that
  unnecessary for the near term

This is lower priority than finishing production adoption, but the design should
not remain permanently ambiguous once the binding cleanup stabilizes.

Acceptance criteria:

- the design direction is explicit: adopt a push-constant-like concept later, or
  intentionally defer it from the active RHI roadmap

---

## 5. Suggested Execution Order

1. Finish the final multi-set production resource layout.
2. Move material-owned resources onto that layout.
3. Refine metadata/schema so the final layout is described explicitly rather
   than tolerated through fallback behavior.
4. Migrate remaining maintained bridge-era call sites.
5. Re-evaluate deferred API questions such as push-constant support only after
   the normal production path is stable.

The important sequencing rule is that compatibility cleanup should follow, not
precede, the point where the final production layout is actually in use.

---

## 6. Main Code Areas Still Likely to Change

Binding model and public API cleanup:

- `src/graphics/interfaces/IShader.h`
- `src/graphics/ShaderBinding.h`
- `src/graphics/ShaderUniformLayout.h`
- `src/graphics/ShaderResourceMetadata.h`

Production renderer/material adoption:

- `src/graphics/Material.h`
- `src/graphics/Material.cpp`
- `src/renderer/passes/ForwardPass.cpp`
- shader source files under `assets/shaders/`, especially `ForwardLit.slang`

Compatibility cleanup and broader adoption:

- older tutorial/demo code under `src/demos/`
- backend fallback handling in:
  - `src/graphics/backends/opengl/GLShader.cpp`
  - `src/graphics/backends/metal/MetalShader.mm`

---

## 7. Success Condition

This design work can be considered complete when all of the following are true:

- the repository's maintained production path uses the intended logical
  multi-set resource layout
- material-owned resources participate in that same model instead of relying on
  `TextureSlot` or pass-local bridge conventions
- backend binding metadata is treated as authoritative production data, not as a
  pilot-only or fallback-assisted mechanism
- flat-slot binding helpers are no longer part of the normal renderer path
- the remaining public API boundary questions have been resolved explicitly

At that point, the repository will no longer be in the "set-aware foundation is
done, broader adoption still pending" stage. It will simply have one real
logical binding model.
