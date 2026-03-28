# Set-Aware Shader Binding Implementation Checklist

This document turns `set_aware_shader_binding_model.md` into an execution
checklist. It is intentionally repository-specific and is meant to be used as
the implementation tracker for the next renderer phase.

Use this together with:

- `docs/design/set_aware_shader_binding_model.md`
- `docs/design/shader_material_system.md`
- `docs/design/uniform_reflection_migration.md`
- `docs/design/metal_backend.md`

---

## 1. Scope and Phase Boundary

This checklist assumes the repository is currently at:

- Phase 5a complete: shader-scoped texture binding exists
- Phase 5b complete: owned uniform buffers exist
- reflected packing is already the mainline path

This checklist covers:

- the set-aware runtime redesign
- backend binding metadata
- the first pilot shader migrations

This checklist does **not** cover:

- `SetPushConstants()`
- the full material-system rewrite
- final cleanup of all legacy tutorial/demo code
- Vulkan runtime implementation

---

## 2. Deliverable Shape

The work should land in six batches:

1. Core binding types and runtime layout keys
2. Reflection/build metadata expansion
3. Set-aware `IShader` API surface
4. OpenGL and Metal backend mapping
5. Test infrastructure and regression coverage
6. Pilot shader migrations (`TexturePreview` -> `ShadowDepth` -> `ForwardLit`)

Recommended rule: each batch should leave the repository buildable and testable.

---

## 3. Batch 1 - Core Binding Types and Runtime Layout Keys

### Goal

Introduce logical resource identity as a first-class runtime concept.

### Files Likely Touched

- `src/graphics/ShaderUniformLayout.h`
- `src/graphics/ShaderUniformLayout.cpp`
- `src/graphics/interfaces/IShader.h`
- new shared type header under `src/graphics/` or `src/graphics/interfaces/`

### Tasks

- Add `ShaderBindingPoint { Set, Binding }`.
- Add `ShaderResourceKind`.
- Add equality/hash helpers required by maps.
- Extend `ShaderUniformBlockLayout` so it stores logical binding identity rather
  than only a flat binding integer.
- Update lookup/storage containers so block layouts can be queried by
  `ShaderBindingPoint`.
- Keep the existing field-name contract unchanged:
  - block-scoped
  - leaf-name based
  - OpenGL/Metal normalize to the same key

### Compatibility Rule

- Keep temporary flat-slot accessors where needed for bridge code.
- Do not remove existing slot-based APIs in this batch.

### Exit Criteria

- runtime layout objects can represent `{set, binding}`
- no reflected field lookup behavior regresses
- existing code still compiles with compatibility helpers in place

---

## 4. Batch 2 - Reflection and Build Metadata Expansion

### Goal

Preserve both logical binding identity and backend-local binding indices in
shader metadata.

### Files Likely Touched

- `cmake/CompileShaders.cmake`
- `src/graphics/backends/metal/MetalShader.mm`
- `src/graphics/backends/opengl/GLShader.cpp`
- possibly new shared metadata parsing helpers under `src/graphics/`

### Tasks

- Define the minimum metadata shape needed at runtime:
  - resource name
  - resource kind
  - logical `{set, binding}`
  - backend-local buffer/texture/sampler indices
  - reflected block field layout where applicable
- Extend Metal reflection loading so `constantBuffer` resources are parsed.
- Extend Metal resource parsing so logical binding metadata is stored separately
  from compacted Metal backend indices.
- Extend the OpenGL path so its runtime metadata also records logical binding
  identity plus the lowered/flattened OpenGL binding/unit index.
- Decide whether this data lives in:
  - one richer sidecar
  - multiple sidecars
  - or one build-generated merged blob

### Important Constraint

- Do not assume Metal `bufferIndex` / `textureIndex` equals logical binding.
- Do not bake backend compaction rules into renderer call sites.

### Exit Criteria

- a shader load yields:
  - logical binding metadata
  - backend-local binding metadata
  - block field layout metadata
- Metal and OpenGL can both answer:
  - "what is this resource logically?"
  - "what backend index do I bind it at?"

---

## 5. Batch 3 - Set-Aware `IShader` API Surface

### Goal

Expose logical binding points in the public shader API without breaking the
current repository in one step.

### Files Likely Touched

- `src/graphics/interfaces/IShader.h`
- shader backend headers:
  - `src/graphics/backends/opengl/GLShader.h`
  - `src/graphics/backends/metal/MetalShader.h`
- callers that query block layouts

### Tasks

- Add set-aware overloads:
  - `BindUniformBuffer(ShaderBindingPoint, Ref<IUniformBuffer>)`
  - `BindTexture(ShaderBindingPoint, Ref<ITexture2D>)`
  - `GetUniformBlockLayout(ShaderBindingPoint)`
- Keep the existing slot-based overloads temporarily as compatibility shims.
- Decide and document the shim rule:
  - flat slot `N` maps to `{set = 0, binding = N}` for bridge code
- Update runtime code that queries layouts so new code can use binding points
  directly.

### Not In Scope

- do not add `SetPushConstants()` here
- do not remove old overloads here

### Exit Criteria

- new code can bind/query by logical binding point
- old bridge code still compiles
- no backend-specific index leaks into public signatures

---

## 6. Batch 4 - Backend Binding Application

### Goal

Make OpenGL and Metal actually consume the logical-to-backend mapping.

### Files Likely Touched

- `src/graphics/backends/opengl/GLShader.cpp`
- `src/graphics/backends/opengl/GLShader.h`
- `src/graphics/backends/metal/MetalShader.mm`
- `src/graphics/backends/metal/MetalShader.h`

### OpenGL Tasks

- Implement set-aware `BindUniformBuffer()` by translating logical binding to the
  lowered OpenGL UBO binding index.
- Implement set-aware `BindTexture()` by translating logical binding to the
  lowered texture unit index.
- Keep the current stage-agnostic texture behavior.

### Metal Tasks

- Build and store:
  - logical binding -> backend buffer index
  - logical binding -> backend texture index
  - logical binding -> backend sampler index
- Update `BindUniformBuffer()` to bind by logical binding and resolve through the
  Metal mapping.
- Update `BindTexture()` to bind by logical binding and resolve through the
  Metal mapping.
- Preserve current per-draw stable semantics for uniform buffers.
- Preserve current "bind both vertex + fragment stage" behavior for textures
  unless a more precise stage model is introduced later.

### Exit Criteria

- OpenGL and Metal both bind resources through the same logical API
- renderer code no longer needs to know backend-local resource indices
- current bridge paths still work

---

## 7. Batch 5 - Test Infrastructure and Regression Coverage

### Goal

Add enough coverage that the binding redesign is safe to build on.

### Files Likely Touched

- `tests/support/FakeRenderBackend.h`
- `tests/unit/TestShaderUniformLayout.cpp`
- `tests/contract/core/TestPassContracts.cpp`
- `tests/contract/core/TestSceneRendererFlow.cpp`
- existing shader integration tests

### Tasks

- Add fake-backend support for logical binding points.
- Add unit tests for:
  - `ShaderBindingPoint` lookup / hashing / equality
  - layout lookup by logical binding
  - logical binding to backend binding mapping behavior
- Add OpenGL/Metal-facing tests for:
  - block layout still resolves the same leaf names
  - backend-local index mapping does not corrupt logical identity
- Keep existing `PackedUniformBlock` tests and ensure they still pass without
  changing the canonical field-name contract.

### Exit Criteria

- fake backend can assert logical binding behavior
- current contract/integration tests pass after the API expansion
- new mapping regressions fail loudly in tests rather than at render time

---

## 8. Batch 6 - Pilot Shader Migrations

### Goal

Validate the redesigned model on real shaders before touching the full material
path.

### Order

1. `TexturePreview`
2. `ShadowDepth`
3. `ForwardLit`

### 8.1 TexturePreview

#### Files Likely Touched

- `assets/shaders/TexturePreview.slang`
- `src/renderer/passes/TexturePreviewPass.cpp`

#### Tasks

- Convert the current implicit block to explicit resource declarations.
- Prefer `ConstantBuffer<T>` plus explicit binding annotations for the first
  pilot, not `ParameterBlock<T>`.
- Move `TexturePreviewPass` to logical binding points.
- Verify:
  - OpenGL field names still normalize to the same leaf names
  - Metal sidecar/resource mapping resolves correctly
  - no backend-local indices leak into pass code

#### Exit Criteria

- `TexturePreviewPass` runs through set-aware bindings end-to-end

### 8.2 ShadowDepth

#### Files Likely Touched

- `assets/shaders/ShadowDepth.slang`
- `src/renderer/passes/ShadowPass.cpp`

#### Tasks

- Apply the same explicit-block strategy proven by `TexturePreview`.
- Move the pass to logical binding points.
- Confirm the same field-name contract and backend mapping logic still holds.

#### Exit Criteria

- `ShadowPass` no longer relies on bridge slot assumptions

### 8.3 ForwardLit

#### Files Likely Touched

- `assets/shaders/ForwardLit.slang`
- `src/renderer/passes/ForwardPass.cpp`

#### Tasks

- Split resources by update-frequency domain:
  - frame/pass
  - material
  - draw, if needed
- Move global/pass textures to explicit logical bindings.
- Keep material-system changes limited; do not roll full material redesign into
  this batch.

#### Exit Criteria

- `ForwardPass` uses the set-aware resource model without introducing the full
  Phase 7 material rewrite

---

## 9. Suggested Commit / PR Boundaries

Recommended split:

1. `Add logical shader binding types and runtime layout keys`
2. `Preserve logical and backend binding metadata in shader reflection`
3. `Add set-aware shader resource binding APIs`
4. `Route OpenGL and Metal binding through logical-to-backend maps`
5. `Add set-aware binding tests and fake backend support`
6. `Migrate TexturePreview to set-aware explicit resource bindings`
7. `Migrate ShadowDepth to set-aware explicit resource bindings`
8. `Migrate ForwardLit to set-aware resource bindings`

This is slightly more granular than the conceptual six batches, but it produces
cleaner review units.

---

## 10. Ready-to-Start Order

If the goal is to begin implementation immediately with the least churn:

1. Batch 1 - Core binding types and runtime layout keys
2. Batch 2 - Reflection/build metadata expansion
3. Batch 3 - Set-aware `IShader` API surface
4. Batch 4 - Backend binding application
5. Batch 5 - Test coverage
6. Batch 6.1 - `TexturePreview`

Do not start `ShadowDepth` or `ForwardLit` before `TexturePreview` proves the
full logical-binding path on both OpenGL and Metal.
