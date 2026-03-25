# Testing Strategy and Evolution Plan

This document defines the testing strategy for RTRLab as the project evolves from
an early rendering playground into a multi-backend rendering lab with stable,
reviewable behavior.

It covers:

- why the current tests are no longer enough on their own
- the target four-layer testing model
- what to add first in this repository
- when tests should be written during day-to-day development
- how OpenGL and Metal backend coverage should be shared and split
- how this should map to CI and local workflow

This document is intended to be the design reference for future test additions
under `tests/`, for backend conformance work, and for deciding which tests are
required when features are added or bugs are fixed.

---

## 1. Current State

The current repository already has useful early tests:

- unit tests under `tests/unit`
- shared contract tests under `tests/contract/core`
- OpenGL contract tests under `tests/contract/opengl`
- OpenGL integration tests under `tests/integration/opengl`
- common helpers under `tests/support`

These tests are valuable, but they mostly reflect the needs of the project's
early phase:

- verifying pure utility logic
- checking a few basic invariants
- checking that OpenGL objects can be created and used without crashing

That was the right starting point, but the repository now contains significantly
more behavior than these tests cover:

- renderer orchestration (`SceneRenderer`)
- render passes (`ForwardPass`, `ShadowPass`, `TexturePreviewPass`)
- file system resolution and config copying
- event bus and scoped subscription lifetime
- backend abstractions (`IGraphicsDevice`, `IRenderCommand`, `IRenderTarget`)
- multiple graphics backends, including Metal

At this stage, "the code did not crash" is no longer enough evidence that the
system is behaving correctly.

The test suite needs to evolve from early smoke coverage into a layered system
that can catch regressions in:

- logic
- state transitions
- backend contracts
- real rendering output

---

## 2. Goals

The testing strategy should achieve the following goals:

- protect core behavior as the renderer grows
- support refactoring without losing confidence
- make regressions easy to localize
- support both OpenGL and Metal, not just one backend
- keep fast tests fast, and expensive tests focused
- make bug fixes leave behind permanent regression coverage

---

## 3. Non-Goals

This strategy does not aim to:

- fully simulate every GPU driver behavior in unit tests
- make every test backend-agnostic at all costs
- replace manual visual review for all rendering changes
- require snapshot testing for every new feature

Rendering still needs some human judgment, but the automated suite should cover
as much stable behavior as possible before manual review is needed.

---

## 4. Testing Model: Four Layers

RTRLab should organize tests into four layers.

### 4.1 Unit Tests

Purpose:

- verify pure logic
- verify math and state updates
- verify edge cases and boundary values
- run fast and often

Typical characteristics:

- no real GPU context required
- no window creation
- no filesystem side effects unless explicitly isolated in a temp directory
- deterministic and cheap

Examples:

- `Camera` basis/vector math
- `Transform` matrix composition
- `Time` accumulation behavior
- `BufferLayout` offset/stride calculation
- `EventBus` publish/unsubscribe semantics
- `FileSystem` path resolution logic

### 4.2 Contract Tests

Purpose:

- verify that modules obey interface contracts
- verify relationships between objects
- verify ownership, delegation, lifecycle, and synchronization rules
- verify behavior at subsystem boundaries without needing full rendering output

Typical characteristics:

- may use fakes, spies, or lightweight backend-independent harnesses
- more structural than unit tests
- faster and narrower than full integration tests

Examples:

- framebuffer resize updates attachment sizes
- render target delegates to its framebuffer when appropriate
- layer stack calls `OnAttach` and `OnDetach` at the correct times
- render passes choose the correct render target and resource path
- `SceneRenderer` executes passes in the correct order

### 4.3 Integration Tests

Purpose:

- verify that multiple real components work together
- exercise the real backend implementation
- validate resource creation, shader loading, render pass execution, and backend setup

Typical characteristics:

- uses a real graphics context
- uses the real backend implementation
- may touch actual shader artifacts and real GPU resources
- more expensive than unit and contract tests

Examples:

- shader compilation and program creation
- texture upload and sampling
- framebuffer creation and resize
- render pass clear/load behavior
- renderer plus render command plus resources working together

### 4.4 Snapshot / Golden Tests

Purpose:

- validate actual rendering results, not just API success
- catch visual regressions that survive lower-layer tests
- provide stable confidence for rendering behavior

Typical characteristics:

- renders to a small off-screen target
- reads back pixels
- compares against a baseline with tolerance
- should focus on stable, intentionally small scenarios

Examples:

- clear color result
- triangle rendering result
- textured quad result
- shadow map preview output
- a minimal forward-lit object under a fixed camera and light

---

## 5. Why These Layers Matter

Each layer catches a different class of failure.

Unit tests catch:

- wrong math
- stale state
- incorrect edge-case handling

Contract tests catch:

- broken ownership and lifecycle rules
- incorrect delegation between abstractions
- render flow regressions that are not purely mathematical

Integration tests catch:

- shader/resource/backend wiring failures
- platform initialization mistakes
- backend-specific API misuse

Snapshot tests catch:

- incorrect final output despite apparently successful API calls
- wrong clear behavior
- wrong texture sampling
- wrong depth interpretation
- subtle pass regressions

No single layer is sufficient on its own.

---

## 6. Repository Structure Target

The repository should evolve toward this testing layout:

```text
tests/
  unit/
  contract/
  integration/
  snapshot/
  support/
```

Suggested responsibilities:

- `tests/unit`
  backend-independent logic and math
- `tests/contract`
  subsystem contracts, state flow, lifecycle, fake-device tests
- `tests/integration`
  real backend tests using real graphics setup
- `tests/snapshot`
  off-screen rendering and pixel comparison
- `tests/support`
  shared fixtures, spies, fake objects, GL/Metal helpers, image comparison utilities

Suggested future CMake targets:

- `rtrlab_unit_tests`
- `rtrlab_contract_tests`
- `rtrlab_contract_tests_opengl`
- `rtrlab_integration_tests_opengl`
- `rtrlab_integration_tests_metal`
- `rtrlab_snapshot_tests_opengl`
- `rtrlab_snapshot_tests_metal`

The exact target naming can vary, but the important point is that backend-specific
test binaries should be separated where necessary.

---

## 7. What the Current Suite Does Well

The current suite provides useful coverage for:

- `Time`
- `Transform`
- `Camera` (including default construction, basis vectors, projections, pitch/yaw constraints)
- `BufferLayout`
- `DebugCameraController` (movement, mouse input, FOV control, null-safety)
- specification/default-value structs (13 tests across all config structs)
- `EventBus` (publish/subscribe, deferred disconnection during dispatch, nested events)
- `FileSystem` (path resolution, config loading, auto-copy behavior)
- `LayerStack` (push/pop ordering, lifecycle callbacks via `LifecycleTrackingLayer`)
- `SceneRenderer` flow (pass execution order, resize, output mode routing via fake backend)
- render pass contracts (`ForwardPass`, `ShadowPass`, `TexturePreviewPass` via `FakeGraphicsDevice`)
- OpenGL framebuffer/render-target contract tests (creation, resize, attachment delegation, pixel readback)
- OpenGL shader integration (compilation, compiled artifact loading, uniform blocks, real draw verification)
- OpenGL texture integration (creation, `SetData`, sampling in real draws, format handling)
- OpenGL render result verification (clear color readback, triangle draw, depth write)
- `SceneRenderer` end-to-end integration (empty scenes, clear colors, rendered output)

This foundation should be kept and expanded, not replaced.

---

## 8. What the Current Suite Is Missing

The remaining major blind spots are:

- backend conformance coverage beyond OpenGL
- snapshot / golden test coverage for rendered output
- negative and error-path coverage beyond shader invalid source
- CI enforcement that integration tests are not silently skipped

Common current gaps:

- Metal is not yet treated as a first-class test target
- no pixel-comparison or image-diff infrastructure for snapshot testing
- limited failure-path testing (e.g., invalid texture formats, null mesh submission)

---

## 9. Priority Plan for This Repository

The most effective strategy is not to test everything evenly.

Testing should be added in priority order:

1. high-risk pure logic and state
2. subsystem contracts and orchestration
3. backend integration behavior
4. snapshot/golden coverage for stable rendering results

### 9.1 P0: Immediate Additions [Done]

These tests were prioritized first because they are high value, relatively
cheap to write, and likely to catch real regressions.

All P0 items have been implemented.

#### Camera [Done]

Files:

- `src/scene/Camera.h`
- `src/scene/Camera.cpp`

Covered in `tests/unit/TestCamera.cpp`:

- default-constructed `Camera` view/projection/view-projection consistency
- `SetPerspective()` updating stored perspective state
- `SetViewportSize(width, 0)` preserving valid state
- extreme pitch behavior not producing invalid matrices
- default camera matrices matching intended initial pose

#### LayerStack [Done]

Files:

- `src/core/app/LayerStack.h`
- `src/core/app/LayerStack.cpp`

Covered in `tests/unit/TestLayerStack.cpp` and `tests/contract/core/TestLayerStackLifecycle.cpp`:

- `PushLayer()` calls `OnAttach()` exactly once
- `PushOverlay()` calls `OnAttach()` exactly once
- `PopLayer()` calls `OnDetach()` exactly once
- `PopOverlay()` calls `OnDetach()` exactly once
- remaining layers are detached during `LayerStack` destruction
- removing a missing layer does not alter stack state

#### EventBus [Done]

Files:

- `src/core/event/EventBus.h`
- `src/core/event/ScopedConnection.h`

Covered in `tests/contract/core/TestEventBus.cpp`:

- basic subscribe/publish flow
- `ScopedConnection` auto-unsubscribe on destruction
- unsubscribe during dispatch (deferred disconnection)
- nested publish behavior
- isolation between event types

#### FileSystem [Done]

Files:

- `src/core/FileSystem.h`
- `src/core/FileSystem.cpp`

Covered in `tests/contract/core/TestFileSystem.cpp`:

- asset path resolution
- saved path resolution
- config resolution priority
- auto-copy from `assets/configs/` to `saved/configs/`
- empty result when config does not exist
- compiled shader directory fallback behavior

#### SceneRenderer Pure Logic [Done]

Files:

- `src/renderer/SceneRenderer.h`
- `src/renderer/SceneRenderer.cpp`

Covered in `tests/contract/core/TestSceneRendererFlow.cpp`:

- `Resize(0, 0)` as no-op
- resize updating tracked dimensions
- directional light view-projection matrix stability for fixed inputs
- output mode switching behavior where testable without full rendering

### 9.2 P1: Contract Tests [Done]

These tests were the next wave, because much of the renderer's risk lives in
relationships and state flow rather than isolated functions.

All P1 items have been implemented. The fake backend infrastructure in
`tests/support/FakeRenderBackend.h` (providing `FakeGraphicsDevice`,
`FakeRenderCommand`, `FakeFramebuffer`, `FakeRenderTarget`, `FakeTexture2D`,
`FakeShader`, etc.) made it possible to test renderer and pass contracts without
requiring a GPU context.

#### GLFramebuffer Contract [Done]

Files:

- `src/graphics/opengl/GLFramebuffer.h`
- `src/graphics/opengl/GLFramebuffer.cpp`

Covered in `tests/contract/opengl/TestFramebuffer.cpp`:

- resize synchronization with attachments
- out-of-range color attachment lookup
- `Resize(0, 0)` no-op
- oversized resize no-op
- integer attachment requirements for `ReadPixel()` and `ClearAttachment()`

#### GLRenderTarget Contract [Done]

Files:

- `src/graphics/opengl/GLRenderTarget.h`
- `src/graphics/opengl/GLRenderTarget.cpp`

Covered in `tests/contract/opengl/TestRenderTarget.cpp`:

- back-buffer target width/height behavior
- framebuffer target delegation behavior
- attachment forwarding correctness
- resize forwarding correctness

#### ForwardPass Contract [Done]

Files:

- `src/renderer/passes/ForwardPass.h`
- `src/renderer/passes/ForwardPass.cpp`

Covered in `tests/contract/core/TestPassContracts.cpp` and
`tests/contract/opengl/TestRenderPasses.cpp`:

- render-to-target mode vs back-buffer mode
- resize behavior
- fallback shadow map path when no shadow map is provided
- skipping null mesh/material items
- albedo texture binding path vs no-texture path

#### ShadowPass and TexturePreviewPass Contract [Done]

Files:

- `src/renderer/passes/ShadowPass.h`
- `src/renderer/passes/ShadowPass.cpp`
- `src/renderer/passes/TexturePreviewPass.h`
- `src/renderer/passes/TexturePreviewPass.cpp`

Covered in `tests/contract/core/TestPassContracts.cpp` and
`tests/contract/opengl/TestRenderPasses.cpp`:

- target size behavior
- resize propagation
- output mode routing
- color-preview vs depth-preview resource selection

#### SceneRenderer Flow Contract [Done]

Files:

- `src/renderer/SceneRenderer.h`
- `src/renderer/SceneRenderer.cpp`

Covered in `tests/contract/core/TestSceneRendererFlow.cpp`:

- pass execution order
- resource handoff between passes
- back-buffer target resize propagation
- correct frame resource preparation

### 9.3 P2: Integration Test Upgrades [Done]

The existing backend tests have been upgraded from smoke coverage to behavior
coverage.

#### Shader Integration [Done]

Files:

- `tests/integration/opengl/TestShader.cpp`
- `src/graphics/opengl/GLShader.cpp`

Covered:

- invalid source fails with a clear exception
- missing compiled shader artifacts fail with a clear exception
- uniform block uploads influence real draw behavior
- compiled shader path really works, not just loads without crashing

#### Texture Integration [Done]

Files:

- `tests/integration/opengl/TestTexture.cpp`
- `src/graphics/opengl/GLTexture2D.cpp`

Covered:

- `SetData()` changes actual sampled pixels
- texture upload survives bind/use in a real draw
- format-specific expectations for color textures

#### Framebuffer / RenderPass Integration [Done]

Files:

- `tests/integration/opengl/TestRenderResults.cpp`
- `src/graphics/opengl/GLRenderCommand.cpp`

Covered:

- clear color produces expected readback values
- depth clear produces expected depth values
- begin/end render pass behavior for back buffer and framebuffer target

#### Minimal Full Rendering Integration [Done]

Covered in `tests/integration/opengl/TestRenderResults.cpp` and
`tests/integration/opengl/TestSceneRenderer.cpp`:

- colored triangle draw
- depth-only render
- SceneRenderer end-to-end with real rendering output

### 9.4 P3: Snapshot / Golden Tests

Once the foundations above exist, snapshot testing becomes highly valuable.

Start with stable, intentionally tiny scenes:

- clear color only
- one triangle
- one textured quad
- `TexturePreviewPass` color output
- `TexturePreviewPass` depth output
- one minimal forward-lit mesh

Snapshot tests should:

- render off-screen
- use very small targets such as 4x4, 8x8, or 16x16
- compare against expected values or reference images with tolerance
- avoid large scenes and unstable inputs

---

## 10. The Missing Backend Requirement: OpenGL and Metal

RTRLab should not treat OpenGL as the only real backend and Metal as an optional
afterthought.

If the repository supports both backends, then the testing strategy must reflect
that explicitly.

### 10.1 Principle

Backend coverage should be divided into:

- backend-independent coverage
- backend-conformance coverage
- backend-specific behavior coverage

### 10.2 Backend-Independent Coverage

These tests should remain shared and backend-agnostic:

- unit tests for math, state, and utility logic
- contract tests for orchestration and ownership
- renderer flow tests that can be expressed through fake devices

Examples:

- `Camera`
- `Transform`
- `Time`
- `EventBus`
- `FileSystem`
- pass ordering in `SceneRenderer`
- `LayerStack` lifecycle

These should run on every platform and should not depend on OpenGL or Metal.

### 10.3 Backend-Conformance Coverage

These tests should exist for every supported backend.

Examples:

- shader creation from source and from compiled artifacts
- texture creation and upload
- framebuffer creation and attachment semantics
- render target behavior
- render pass clear/load/store behavior
- draw-call path sanity

This should become a shared test concept implemented against:

- OpenGL on supported OpenGL platforms
- Metal on macOS

The important design goal is not "write two unrelated suites", but "define one
backend conformance matrix and run it against each backend where supported."

### 10.4 Backend-Specific Coverage

Some tests should exist only for one backend because the behavior is inherently
backend-specific.

#### OpenGL-Specific Examples

- GL object lifetime assumptions
- integer attachment readback semantics
- GL debug callback paths
- OpenGL-specific format behavior

#### Metal-Specific Examples

- command buffer and encoder lifecycle
- drawable acquisition and present path
- Metal texture format mapping, including `RGB8` padding behavior
- `Depth24Stencil8` fallback behavior to Metal-native depth/stencil formats
- pipeline state cache behavior
- MSL shader loading and reflection metadata path

Metal-specific tests are not optional if Metal is a supported backend.

---

## 11. Proposed OpenGL / Metal Test Matrix

The test suite should evolve toward the following matrix.

| Layer | OpenGL | Metal | Notes |
|------|--------|-------|-------|
| Unit | Yes | Yes | Shared, backend-independent |
| Contract | Yes | Yes | Mostly shared; use fake devices when possible |
| Integration | Yes | Yes | Real backend implementation |
| Snapshot | Yes | Yes | Backend-specific baselines may be needed |

### 11.1 Unit Layer and Metal

The Metal backend does not need its own unit tests for backend-independent math,
but Metal-side helper logic should still get unit coverage where possible.

Examples:

- format conversion helpers in `src/graphics/metal/MetalTypes.h`
- backend selection logic
- small CPU-side state mapping utilities

### 11.2 Contract Layer and Metal

Contract tests should verify that Metal backend abstractions obey the same RHI
contracts as OpenGL.

Examples:

- `IRenderTarget` behavior
- framebuffer resize semantics
- backend device factory behavior
- render command lifecycle rules

The contract should be shared even when implementation details differ.

### 11.3 Integration Layer and Metal

A Metal integration suite should eventually cover at least:

- Metal device creation
- command queue / command buffer setup
- shader creation from compiled MSL or metallib
- texture creation and upload
- framebuffer/render target creation
- render pass execution
- minimal draw to an off-screen target
- pixel readback from off-screen render targets

This suite should run on macOS CI and on developer machines that build the Metal backend.

### 11.4 Snapshot Layer and Metal

Metal snapshot tests should mirror the same rendering scenarios as OpenGL where
possible:

- clear color
- triangle
- textured quad
- texture preview
- minimal lit mesh

It is acceptable to keep separate baselines per backend when output differences
are real and stable.

The goal is not bit-identical output across all APIs at all costs.

The goal is:

- each backend produces stable, correct output for the same scenario
- backend-specific regressions are caught early

---

## 12. Designing Shared Backend Conformance Tests

To avoid duplicating too much test logic, backend integration tests should be
written around shared test scenarios and backend-specific fixture setup.

Suggested pattern:

- define common scenario helpers in `tests/support/`
- provide backend-specific context fixtures
- instantiate the same test intent for OpenGL and Metal where practical

For example:

```text
tests/support/
  RenderBackendTestScenario.h
  GLTestContext.h/.cpp
  MetalTestContext.h/.mm
```

Then implement common scenario functions such as:

- create a 2x2 RGBA texture and upload known pixels
- create a framebuffer with one color attachment
- clear a render target to a known color
- draw a simple triangle
- read back a pixel and compare

The backend-specific fixture is responsible for:

- creating the real backend context
- installing the proper device
- handling platform-specific setup/teardown

The test scenario is responsible for:

- describing the behavior expected from any backend

This gives the repository a true backend conformance suite instead of two
totally separate test worlds.

---

## 13. Snapshot Baseline Policy

Snapshot tests are powerful, but they become fragile if unmanaged.

RTRLab should use the following baseline rules:

- keep scenes small and stable
- keep camera, lights, viewport size, and clear color fixed
- compare with tolerance, not strict raw equality
- version baselines by backend if necessary
- review baseline updates explicitly in code review

Suggested directory:

```text
tests/baselines/
  opengl/
  metal/
```

Suggested baseline categories:

- `clear_color_*`
- `triangle_*`
- `textured_quad_*`
- `texture_preview_color_*`
- `texture_preview_depth_*`
- `forward_lit_minimal_*`

---

## 14. Test Support Infrastructure

The repository has gained dedicated test support components under `tests/support/`.

### Implemented

- `FakeRenderBackend.h`
  consolidated fake backend providing `FakeGraphicsDevice`, `FakeRenderCommand`,
  `FakeFramebuffer`, `FakeRenderTarget`, `FakeTexture2D`, `FakeShader`,
  `FakeVertexBuffer`, `FakeIndexBuffer`, and `FakeVertexArray`.
  `FakeRenderCommand` records all draw calls, state changes, texture binds,
  and viewports, serving the role originally envisioned for a separate
  `RenderSpy`.
- `GLTestContext.h/.cpp`
  creates a hidden OpenGL context for contract and integration tests, provides
  a global device context via `GetDevice()`.
- `MathTestUtils.h`
  floating-point comparison helpers (`ExpectVec3Near`, `ExpectMat4Near`,
  `ExpectFloatNear`).
- `TestLayer.h`
  mock layer for `LayerStack` unit tests (tracks live/destroyed counts).
- `LifecycleTrackingLayer.h`
  layer that records `OnAttach`/`OnDetach` callbacks via a shared state object.

### Not Yet Implemented

- `PixelReadback.h/.cpp`
  shared helper for backend-specific pixel readback (currently done inline in
  integration tests).
- `ImageCompare.h/.cpp`
  tolerance-based image comparison for snapshot tests.
- `MetalTestContext.h/.mm`
  real Metal test fixture.

These remaining helpers will become necessary when the project adds snapshot
testing (Phase E) and Metal integration testing (Phase D).

---

## 15. When Tests Should Be Written During Development

There is no single universal moment to write tests.

The right timing depends on the type of work.

### 15.1 New Pure Logic

For new logic-heavy code, tests should usually be written first or written
alongside implementation.

Examples:

- math helpers
- event routing
- path resolution
- state machines

Recommended workflow:

1. define the behavior
2. write the unit test
3. implement until the test passes
4. refactor with the test protecting behavior

### 15.2 New Renderer Flow or Pass Logic

For new orchestration code, contract tests should usually be written very early,
often before or during implementation.

Examples:

- new render pass
- new frame-resource routing
- output mode switching
- backend abstraction changes

Recommended workflow:

1. define what the pass or subsystem must do
2. write contract tests for resource flow and state transitions
3. implement the real code
4. add integration coverage for the backend path

### 15.3 Bug Fixes

For bugs, the best workflow is:

1. write a failing test that reproduces the bug
2. fix the bug
3. keep the new test permanently

This gives the repository regression memory.

Every important bug should leave behind a test when practical.

### 15.4 Refactoring Old Code

For refactors, the right first move is often not a new feature test, but a
characterization test.

A characterization test says:

"whatever the current behavior is, I want to pin it down before I reorganize the code."

This is especially useful for:

- renderer flow
- file system behavior
- backend setup code
- pass interactions

### 15.5 Early Prototyping

In early prototype spikes, it is acceptable to write little or no test code.

But once the code becomes:

- reusable
- relied on by other systems
- non-trivial to reason about
- intended to stay

it should gain tests before further expansion.

---

## 16. Recommended Day-to-Day Development Workflow

The project should use a practical, layered workflow rather than a rigid rule.

### 16.1 For a New Feature

Recommended flow:

1. decide which behavior is a stable contract
2. add unit tests for pure logic
3. add contract tests for subsystem behavior
4. implement the feature
5. add integration or snapshot coverage if GPU/backend output matters

### 16.2 For a Backend Change

Recommended flow:

1. identify whether the change affects shared RHI contract or backend-specific behavior
2. update shared contract tests if the contract changes
3. add or update backend integration tests
4. add or update snapshots if final rendering output changes
5. run the backend matrix relevant to the change

### 16.3 For a Bug Fix

Recommended flow:

1. reproduce with a test
2. fix the code
3. run the narrow test locally
4. run the relevant suite layer
5. keep the regression test

### 16.4 For a PR

Every meaningful PR should add at least one of:

- unit coverage
- contract coverage
- integration coverage
- snapshot coverage

The right layer depends on the kind of change.

Not every change needs all four layers.

But feature work should not merge with no test strategy at all.

---

## 17. CI Strategy

The CI pipeline should eventually be layered the same way as the test suite.

### 17.1 Fast Path

Run on every PR:

- unit tests
- contract tests

These should be cheap and deterministic.

### 17.2 Backend Integration Path

Run on backend-capable CI jobs:

- OpenGL integration tests on an environment that supports the OpenGL path
- Metal integration tests on macOS

### 17.3 Snapshot Path

Run on:

- nightly builds
- backend-specific jobs
- or PRs that touch rendering output significantly

Snapshot tests are valuable, but they are typically more expensive and may be
more sensitive to environment details.

### 17.4 Suggested Platform Matrix

Minimum target matrix:

- Windows: shared tests + OpenGL integration/snapshot
- macOS: shared tests + Metal integration/snapshot

Optional future matrix:

- macOS OpenGL where still supported for development comparison
- Linux OpenGL if the project adds Linux support

---

## 18. Recommended First Ten Tests

These were originally identified as the fastest path to higher confidence.
Nine of the ten have been implemented.

1. [Done] `Camera` default construction matrix consistency — `tests/unit/TestCamera.cpp`
2. [Done] `LayerStack` lifecycle callback tests — `tests/contract/core/TestLayerStackLifecycle.cpp`
3. [Done] `EventBus` unsubscribe-during-dispatch test — `tests/contract/core/TestEventBus.cpp`
4. [Done] `FileSystem::ResolveConfigPath()` priority and auto-copy test — `tests/contract/core/TestFileSystem.cpp`
5. [Done] framebuffer resize no-op on invalid sizes — `tests/contract/opengl/TestFramebuffer.cpp`
6. [Done] integer attachment clear/read contract test — `tests/contract/opengl/TestFramebuffer.cpp`
7. [Done] render pass clear color readback integration test — `tests/integration/opengl/TestRenderResults.cpp`
8. [Done] invalid shader source failure integration test — `tests/integration/opengl/TestShader.cpp`
9. [Done] `ForwardPass` fallback shadow map contract test — `tests/contract/core/TestPassContracts.cpp`
10. [Not Started] one small off-screen triangle snapshot test — requires snapshot infrastructure (Phase E)

The next priorities are Metal backend conformance (Phase D) and snapshot
infrastructure (Phase E).

---

## 19. Suggested Implementation Roadmap

### Phase A: Strengthen Foundations [Done]

Added:

- `EventBus` tests (`tests/contract/core/TestEventBus.cpp`)
- `FileSystem` tests (`tests/contract/core/TestFileSystem.cpp`)
- `LayerStack` lifecycle tests (`tests/contract/core/TestLayerStackLifecycle.cpp`)
- missing `Camera` coverage (`tests/unit/TestCamera.cpp`)

Outcome:

- strong unit layer
- stronger basic correctness confidence

### Phase B: Add Contract Layer [Done]

Added:

- fake backend infrastructure in `tests/support/FakeRenderBackend.h`
- `SceneRenderer` flow tests (`tests/contract/core/TestSceneRendererFlow.cpp`)
- render pass contract tests (`tests/contract/core/TestPassContracts.cpp`)
- render target/framebuffer delegation tests (`tests/contract/opengl/TestRenderTarget.cpp`, `tests/contract/opengl/TestFramebuffer.cpp`)

Outcome:

- renderer behavior is testable without overusing GPU tests

### Phase C: Upgrade OpenGL Integration [Done]

Added:

- real readback-based result tests (`tests/integration/opengl/TestRenderResults.cpp`)
- shader failure-path tests (`tests/integration/opengl/TestShader.cpp`)
- small render-chain tests (`tests/integration/opengl/TestSceneRenderer.cpp`)

Outcome:

- OpenGL backend has moved from smoke coverage to behavior coverage

### Phase D: Add Metal Integration [Not Started]

Add:

- Metal test fixture
- Metal backend conformance suite

Outcome:

- Metal becomes a real tested backend, not just a compiled backend

### Phase E: Add Snapshot / Golden Tests [Not Started]

Add:

- `PixelReadback` and `ImageCompare` helpers in `tests/support/`
- tiny stable off-screen scenes
- backend-specific baselines where appropriate

Outcome:

- visual regressions become catchable in automation

---

## 20. Key Principles

The test strategy for RTRLab should follow these principles:

1. fast tests should stay fast
2. expensive tests should be focused and intentional
3. behavior is more important than call success
4. contracts should be tested before full rendering output when possible
5. bug fixes should leave behind regression tests
6. backends are not equally optional if they are equally supported
7. OpenGL and Metal should both be part of the testing story

---

## 21. Final Policy Summary

For this repository, the testing policy should be:

- use unit tests for pure logic
- use contract tests for subsystem behavior and RHI rules
- use integration tests for real backend cooperation
- use snapshot tests for stable rendered output
- keep backend-independent logic shared
- require backend-conformance testing for both OpenGL and Metal
- allow backend-specific tests where implementations genuinely differ

The project should not aim for "some tests exist."

It should aim for:

- the right tests at the right layer
- the right backend coverage for supported platforms
- a workflow where tests grow naturally with the codebase

---

## References

- `tests/unit`
- `tests/contract/core`
- `tests/contract/opengl`
- `tests/integration/opengl`
- `tests/support`
- `src/renderer/SceneRenderer.cpp`
- `src/renderer/passes/ForwardPass.cpp`
- `src/renderer/passes/ShadowPass.cpp`
- `src/renderer/passes/TexturePreviewPass.cpp`
- `src/core/FileSystem.cpp`
- `src/core/event/EventBus.h`
- `src/graphics/opengl/GLFramebuffer.cpp`
- `src/graphics/opengl/GLRenderTarget.cpp`
- `src/graphics/opengl/GLRenderCommand.cpp`
- `docs/design/metal_backend.md`
- `docs/design/shader_material_system.md`
