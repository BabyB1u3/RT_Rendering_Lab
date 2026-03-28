# Metal Backend - Remaining Work Plan

This document no longer describes the initial Metal backend bring-up.
The core Metal runtime, shader loading path, reflection sidecar loading,
set-aware binding resolution, and ImGui integration already exist in the
repository.

This file tracks only the work that is still incomplete after the initial
backend landed.

**Prerequisites**:

- `docs/design/shader_system.md`
- `docs/design/material_system.md`
- `docs/design/shader_binding.md`
- `docs/design/set_aware_shader_binding_model.md`
- `docs/design/resource_packaging.md`
- `docs/design/testing_strategy.md`

---

## 1. Scope

### 1.1 What This Document Covers

- the remaining migration from the current transitional Metal path to the final
  material/resource architecture
- cleanup of temporary compatibility layers that still exist for bridge/demo code
- testing, CI, and performance hardening work that is still missing for Metal

### 1.2 What This Document Intentionally Omits

The following are already implemented and are therefore not restated here:

- Metal implementations of the core RHI interfaces
- window/device bring-up for Metal on macOS
- build-time Slang -> `.metal` compilation
- runtime loading of `.metal` shaders into `MTLLibrary`
- reflection sidecar emission and loading
- logical-to-backend binding resolution inside `MetalShader`
- current pass-level usage of reflected layouts plus `PackedUniformBlock`
- Dear ImGui Metal backend integration

### 1.3 Non-Goals

- iOS / iPadOS / visionOS support
- Vulkan backend work
- Metal-specific feature expansion beyond what the current RHI needs
- rewriting the working Metal path just for stylistic consistency

---

## 2. Current Gap Summary

The repository is no longer blocked on basic Metal support.
The remaining gap is architectural:

- renderer and material code have not fully moved to the final multi-set logical
  resource model
- some compatibility APIs and demo paths still use flat slot assumptions
- Metal is not yet treated as a fully verified backend in tests and CI
- some performance and robustness work remains in provisional form

In practice, the unfinished work is less about "making Metal render" and more
about making the Metal path:

- the same resource model the renderer is supposed to target long-term
- test-covered like OpenGL
- hardened enough to stop relying on transitional behavior

---

## 3. Remaining Goals

### 3.1 Finish the Final Material / Resource Organization

The highest-priority remaining task is to complete the migration from the current
flat set-0 convention to the intended multi-set layout described in
`material_system.md` and `set_aware_shader_binding_model.md`.

Target shape:

- per-frame / per-pass resources live in set 0
- per-material resources live in set 1
- per-draw resources live in set 2
- renderer code binds by logical `set + binding`, not by compatibility slot

What is still missing:

- `Material` is still an older property bag with `TextureSlot` assumptions and a
  legacy `UploadToShader()` model
- pass code still manually pulls material properties and writes them into a
  shared pass-owned block
- current production passes still use the transitional flat `{0, N}` layout
  instead of the final multi-set organization

Required changes:

- redesign `Material` around reflected resource ownership rather than fixed
  texture-unit enums
- define which fields/resources are authored as per-material vs per-draw
- update shaders so their logical bindings match the final set layout
- update renderer passes to bind frame/pass, material, and draw data separately
- migrate demos and sample content to the new convention

Acceptance criteria:

- `ShaderBindingSets::FramePass`, `ShaderBindingSets::Material`, and
  `ShaderBindingSets::Draw` are used by real renderer code, not just declared
  as future constants
- `ForwardPass`, `ShadowPass`, and related material-driven paths no longer rely
  on the flat set-0 compatibility convention
- material uploads are organized by logical ownership instead of pass-local
  ad hoc extraction

### 3.2 Complete the Reflection-Driven Upload Path Everywhere

The repository has the core reflected packing path, but some older upload paths
still remain in demos and compatibility code.

The intended end state is:

- bytes uploaded to uniform/storage resources come from reflected layout metadata
- gameplay/render code does not hand-author backend-sensitive padding rules
- passes and demos do not depend on raw C++ mirror structs matching backend
  layout by accident

What is still missing:

- some tutorial/demo code still uses raw `SetUniformBlock(binding, data, size)`
- some code still depends on the bridge layer instead of treating reflected
  layouts as authoritative
- the material path is not yet fully expressed through reflected per-resource
  layouts

Required changes:

- migrate remaining demo/tutorial code off handwritten raw block uploads where
  those blocks are meant to be portable
- keep `PackedUniformBlock` or generated layout code as the canonical packing
  mechanism
- make the final material/resource system consume the same reflected metadata
  path as the passes that already migrated

Acceptance criteria:

- no production rendering path depends on handwritten backend-specific mirror
  structs for portable shader data
- remaining uses of `SetUniformBlock()` are either eliminated or clearly limited
  to temporary compatibility/test scenarios

### 3.3 Remove Transitional Compatibility Layers

The repository still carries bridge-era APIs to keep older code running while the
migration proceeds.

Those shims were useful, but they should not remain the long-term renderer model.

Still present today:

- flat-slot helpers that map slot `N` -> logical `{0, N}`
- deprecated flat overloads on `IShader`
- demo/tutorial call sites that still rely on the bridge behavior

Required changes:

- migrate remaining call sites to explicit `ShaderBindingPoint`
- remove or sharply narrow flat-slot compatibility helpers once production code
  no longer depends on them
- update tests so they validate the final logical binding model rather than the
  transition layer

Acceptance criteria:

- renderer and demo code bind through explicit logical binding points
- compatibility overloads are either removed or quarantined to clearly temporary
  compatibility-only code

### 3.4 Decide Whether `SetPushConstants()` Is Still Needed

There is still no `SetPushConstants()` API in `IShader`.

That is not blocking the current Metal backend, but it remains an open design
question for the future cross-backend RHI.

Required decision:

- either add a real push-constant style API with clear backend semantics
- or explicitly defer/remove it from the near-term plan if the material/draw
  model makes it unnecessary

Acceptance criteria:

- the design docs stop treating push constants as an implicit future feature
- the RHI direction is explicit: adopted now, or intentionally postponed

---

## 4. Verification and Delivery Work

### 4.1 Add Metal-Specific Tests

Metal is still underrepresented in the test matrix.
The repository currently has shared tests and OpenGL-specific test targets, but
not parallel Metal contract/integration coverage.

Missing areas:

- Metal backend contract tests
- Metal integration tests on macOS
- Metal-side validation of reflection-driven binding/layout behavior
- format/readback behavior specific to Metal

Minimum coverage to add:

- device/shader creation on Metal
- logical binding resolution for buffers/textures/samplers
- reflected uniform block layout loading on Metal
- `RGB8` upload padding behavior
- `Depth24Stencil8` fallback behavior
- framebuffer readback behavior
- per-draw stable uniform-buffer semantics when one logical buffer is reused

Acceptance criteria:

- the project builds dedicated Metal test targets on macOS
- Metal-specific behavior is covered by automated tests, not just manual demos

### 4.2 Add macOS CI for the Metal Backend

The repository does not yet treat Metal as a continuously verified delivery
target.

Required changes:

- add a macOS CI job that configures and builds the Metal backend
- run at least the shared tests plus Metal-specific suites on macOS
- ensure compiled shader artifacts are generated in CI as part of the build

Acceptance criteria:

- pull requests exercise the Metal backend automatically on macOS
- Metal regressions are caught without depending on local manual testing

---

## 5. Hardening and Performance Work

### 5.1 Replace Provisional Sync Paths Where Necessary

Some current Metal operations are correct but intentionally conservative.

Known provisional areas:

- texture upload/mipmap generation still use synchronous command-buffer waits
- framebuffer readback is synchronous
- dynamic vertex/uniform resource management is not yet organized around the
  fuller ring-buffer / multi-frame strategy originally sketched during design

Required changes:

- review whether the current per-draw snapshot strategy is sufficient for
  expected workloads or whether a ring-buffer style upload path is needed
- reduce unnecessary `waitUntilCompleted()` usage on non-editor-critical paths
- keep correctness guarantees intact while improving throughput

Acceptance criteria:

- no correctness regressions in per-draw resource semantics
- obvious synchronous stalls are removed or clearly confined to tooling paths

### 5.2 Improve Runtime Diagnostics

The backend would benefit from stronger operational diagnostics.

Remaining work:

- surface `MTLCommandBuffer` failure/status callbacks in engine logging
- document or enable Metal validation-layer workflows for debugging
- ensure common backend failures produce actionable logs

Acceptance criteria:

- backend failures are easier to diagnose than "shader failed" or "draw failed"
- Metal validation becomes part of normal backend debugging workflow

### 5.3 Optional Offline `.metallib` Packaging

Runtime source compilation is functional today, but offline packaging remains a
useful follow-up optimization.

Potential work:

- compile `.metal` sources into `.metallib` during the build or asset pipeline
- load binary libraries at runtime instead of compiling source text

This is lower priority than architecture cleanup, testing, and CI.

Acceptance criteria:

- if implemented, the packaging path is integrated into the existing shader
  build/copy flow and does not fork the runtime model unpredictably

---

## 6. Suggested Execution Order

1. Finish the final multi-set material/resource layout.
2. Migrate remaining demos and rendering code off transitional flat-slot usage.
3. Add Metal contract/integration tests on macOS.
4. Add macOS CI for the Metal backend.
5. Harden performance/diagnostics once the architecture and test surface are stable.
6. Re-evaluate optional work such as push constants and `.metallib`.

The key dependency is that test and CI work should validate the architecture we
actually want to keep, not just the bridge state we used during migration.

---

## 7. Main Code Areas Still Likely to Change

Renderer/material migration:

- `src/graphics/Material.h`
- `src/graphics/Material.cpp`
- `src/renderer/passes/ForwardPass.cpp`
- `src/renderer/passes/ShadowPass.cpp`
- `src/renderer/passes/TexturePreviewPass.cpp`
- material-driven demos and scene setup code

Shader/RHI cleanup:

- `src/graphics/interfaces/IShader.h`
- `src/graphics/ShaderBinding.h`
- `src/graphics/ShaderUniformLayout.h`
- shader source files under `assets/shaders/`

Metal verification/hardening:

- `tests/CMakeLists.txt`
- future Metal test fixtures and suites under `tests/`
- `src/graphics/backends/metal/MetalShader.mm`
- `src/graphics/backends/metal/MetalRenderCommand.mm`
- `src/graphics/backends/metal/MetalTexture2D.mm`
- `src/graphics/backends/metal/MetalFramebuffer.mm`

---

## 8. Success Condition

This document can be retired once all of the following are true:

- the renderer uses the final logical multi-set binding model in production code
- material and draw uploads are fully reflection-driven
- flat-slot bridge behavior is no longer a normal production path
- Metal has dedicated tests and macOS CI coverage
- remaining synchronous/provisional backend behavior has been either improved or
  consciously accepted as final behavior
