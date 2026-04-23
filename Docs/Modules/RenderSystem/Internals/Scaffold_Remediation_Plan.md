# Render System Scaffold Remediation Plan

Last updated: 2026-04-23

Related documents:
- [../Design/RHI.md](../Design/RHI.md)
- [../Design/RHI_Backend_Vulkan.md](../Design/RHI_Backend_Vulkan.md)
- [../Design/RHI_Backend_Metal.md](../Design/RHI_Backend_Metal.md)
- [../Design/ShaderSystem.md](../Design/ShaderSystem.md)
- [../CodeReview/RHI.md](../CodeReview/RHI.md)

---

## 1. Purpose

This document turns the current render-system cleanup discussion into an execution plan.

The immediate goal is not to build a full renderer. The goal is to move the project from a demo-oriented bring-up scaffold to a trustworthy rendering foundation that can support a minimal renderer without fighting the RHI every step of the way.

In short:

- current state: RHI bring-up skeleton with many temporary paths
- target state: minimal-renderer-ready rendering base

---

## 2. Target End State

The render system should be considered "scaffold remediation complete" when all of the following are true:

- the public RHI no longer exposes fake capabilities
- Vulkan and Metal agree on the public graphics-path contract
- shader reflection, pipeline layout translation, resource binding, and command recording form a closed loop
- the frame model supports real renderer-owned pass recording instead of application-owned demo recording
- the backends support the minimum graphics feature set required for a real renderer

That minimum feature set is:

- multiple frames in flight
- offscreen render targets
- depth attachments
- MRT
- real texture and buffer barriers
- real transient upload path
- correct texture, sampler, buffer, and constant binding
- correct sampled-texture versus storage-texture semantics

Practical outcome:

- a minimal renderer can render textured meshes
- multiple objects can be drawn with per-frame, per-material, and per-object data
- at least one offscreen pass can feed a final present pass
- Vulkan and Metal can run the same renderer-side flow above the RHI boundary

This is roughly "M4 closed out properly plus M5 ready to land", not a production renderer.

---

## 3. What This Plan Does Not Try To Solve

This plan intentionally does not target:

- render graph integration
- material-system expansion
- shader hot reload
- bindless-first abstractions
- advanced residency or aliasing systems
- large-scale renderer features such as shadows, lighting, visibility, or streaming

Those can follow after the render system becomes trustworthy.

---

## 4. Execution Principles

### 4.1 Fix honesty before fix completeness

The first priority is removing misleading behavior:

- no shell fallback that makes unsupported features look implemented
- no public API entry that silently does nothing
- no comments or docs that describe a capability the code does not actually have

### 4.2 Fix shared abstractions before backend detail

If a public contract is wrong, backend work on top of it will be throwaway work.

The correct order is:

1. public contract
2. data model and reflection flow
3. frame and pass ownership
4. backend completion

### 4.3 Keep Vulkan and Metal aligned at milestone level

Vulkan may land a detailed implementation slightly earlier inside a milestone, but the public API must not drift into a Vulkan-only truth. Metal must stay close enough that the shared abstraction remains real.

### 4.4 Every stage must have an exit criterion

No stage is done just because code was refactored. Each stage ends only when a concrete capability boundary or validation boundary has been reached.

---

## 5. Main Workstreams

The remediation naturally breaks into these workstreams:

- public API honesty and shell fallback removal
- shader reflection and resource-model correctness
- frame lifetime and pass ownership redesign
- upload and copy infrastructure
- Vulkan graphics-path completion
- Metal graphics-path alignment
- documentation, cleanup, and validation

These workstreams are interdependent, so the recommended execution order is staged below.

---

## 6. Staged Plan

## Phase 0 - Freeze Scope And Success Criteria

### Objective

Define exactly what this remediation pass is trying to complete, and just as importantly, what it is not trying to complete.

### Work

- write down the supported feature set for this pass
- classify each exposed public RHI feature as:
  - supported
  - explicitly unsupported
  - deferred to a later milestone
- decide the short-term stance for compute:
  - recommended: explicitly unsupported in public behavior until fully implemented
- decide the short-term stance for upload:
  - recommended: introduce a minimal real copy/upload path rather than keeping `WriteBuffer` as the implied future
- define the minimal renderer demo that will be used as the end-state validation target

### Deliverables

- a support matrix for current and target public RHI capabilities
- agreement on the "done means this" minimal renderer scenario

### Exit Criteria

- every public API surface is categorized
- the team has a single agreed completion target

---

## Phase 1 - Make The Public RHI Honest

### Objective

Remove all behavior where the interface claims more than the implementation provides.

### Why This Comes First

The current codebase contains several cases where unsupported features appear valid because they fall back to shell implementations or cache bytes locally without issuing backend work. That causes hidden design debt and invalid downstream assumptions.

### Work

- remove or explicitly block shell fallback for unimplemented production-backend features
- make unimplemented compute paths fail loudly instead of succeeding superficially
- make unimplemented push-constant paths fail loudly instead of storing data in a dead local cache
- stop relying on no-op barrier implementations when higher-level code assumes transitions were emitted
- decide and document the canonical texture-binding contract:
  - either require `TextureView*`
  - or support `Texture*` by resolving a canonical backend view
- audit assert text and comments so they describe current truth
- update outdated "bring-up" and milestone markers where they are now misleading

### Highest-Priority Items

- `CreateComputePipeline`
- `BindComputePipeline`
- `Dispatch`
- `PushConstants`
- `TextureBarrier`
- `BufferBarrier`
- texture binding contract between `ShaderParameterWriter`, `ResourceSet`, and Vulkan

### Deliverables

- unsupported features now fail explicitly
- no production backend relies on shell behavior for a public feature that appears implemented

### Exit Criteria

- there are no remaining fake-success paths in the public RHI
- a caller can no longer accidentally depend on unsupported behavior

---

## Phase 2 - Close The Shader And Resource Binding Model

### Objective

Make reflection, pipeline layout derivation, resource binding, and constant-data upload semantically correct.

### Why This Comes Before Renderer Work

If shader semantics are lost or translated incorrectly, every later renderer feature will be built on false assumptions.

### Work

- extend reflection ingestion to carry all required semantics through the pipeline
- correctly distinguish:
  - sampled textures
  - storage textures
  - buffers
  - samplers
  - parameter blocks
  - push constants
- preserve array counts from reflection instead of collapsing them to `1`
- verify whether buffer semantics need a richer neutral representation than the current `Buffer` classification
- ensure `BuildPipelineLayoutDescFromReflection()` preserves shader truth rather than a demo-friendly subset
- fix `ShaderParameterWriter` so it can bind textures using the backend contract chosen in Phase 1
- make `ResourceSet` size and constant-storage behavior reflect declared constant ranges, not just bytes written so far
- add layout-compatibility validation when binding resource sets against the currently bound pipeline

### Recommended Scope For This Phase

Required in this phase:

- parameter-block constants
- sampled textures
- samplers
- storage buffers
- push constant ingestion

Strongly recommended in this phase:

- storage textures
- resource arrays

If resource arrays cannot be fully implemented in this phase, the data model must still preserve their semantics and reject unsupported runtime usage explicitly rather than erasing the information.

### Deliverables

- reflection data model no longer loses shader meaning
- pipeline layout derivation becomes trustworthy
- `ShaderParameterWriter` becomes a real front door rather than a demo helper

### Exit Criteria

- a shader's declared resource interface reaches the backend without semantic collapse
- layout, bindings, and constants are correct by construction

---

## Phase 3 - Redesign Frame And Pass Ownership

### Objective

Move from application-owned demo recording to renderer-owned explicit frame and pass recording.

### Problem Being Solved

The current model has `Application` implicitly opening and closing the backbuffer rendering scope and exposing a transient command list to layers. That works for a single demo pass, but it locks the architecture into the wrong ownership model for a real renderer.

### Work

- reduce `Application` to frame pumping and platform integration
- remove the assumption that `Application` owns the outer render pass
- remove the transitional "current command list" global-style accessor
- introduce a renderer-facing frame context or render context
- make pass construction explicit:
  - who creates `RenderingInfo`
  - who chooses attachments
  - who owns pass begin/end
  - who decides barrier flush points
- allow the backbuffer pass to become one explicit renderer pass among others
- prepare the frame model for:
  - offscreen pass
  - main scene pass
  - present pass

### Design Recommendation

Recommended direction:

- `Application` owns the frame loop
- a thin renderer coordinator owns pass recording and submission structure
- demos or layers issue render work through that renderer-facing context, not through application internals

### Deliverables

- explicit frame and pass ownership
- no hidden render scope held by `Application`

### Exit Criteria

- a frame can record more than one pass without architectural workarounds
- backbuffer rendering is no longer hardcoded as the application's implicit pass

---

## Phase 4 - Implement Real Frame Resources And Upload Infrastructure

### Objective

Replace the single-frame bring-up lifetime model with real per-frame ownership and a real upload path.

### Problem Being Solved

The current model still assumes:

- one command list
- one command buffer
- one frame slot
- one tiny upload arena
- a demo-only direct host write path

That is sufficient for a triangle demo and insufficient for a renderer.

### Work

- implement multiple in-flight frame slots in Vulkan and Metal
- give each frame slot its own command recording and synchronization ownership
- replace single-slot upload assumptions with real per-frame transient allocation
- remove fixed-capacity upload behavior that only asserts on overflow
- allow static resource sets to reuse uploaded data where valid
- ensure dynamic per-object uploads are isolated per frame slot
- implement the minimal public copy/upload path required to support staging

### Minimum Recommended API Additions

At minimum, introduce enough command functionality to support a real upload path:

- `CopyBuffer`
- `CopyBufferToTexture`

Optional but likely useful:

- `CopyTexture`

### Fate Of `WriteBuffer`

Recommended outcome:

- retain `WriteBuffer` only as a narrow compatibility path for simple CPU-visible buffers
- stop treating it as the future upload architecture

### Deliverables

- double-buffered or triple-buffered frame model
- real transient upload allocator behavior
- a real copy path for staging-based initialization and uploads

### Exit Criteria

- frames in flight are real rather than emulated
- uploads and initialization no longer depend on direct host writes to production resources

---

## Phase 5 - Complete Vulkan To Minimal-Renderer Level

### Objective

Bring the Vulkan backend from swapchain-only demo rendering to a real minimal renderer backend.

### Work

- support rendering to device-created color targets
- support depth attachments
- support MRT
- generalize image layout transitions beyond swapchain color images
- make aspect, mip, and array-layer handling correct
- implement real texture and buffer barriers
- align descriptor writes with true resource semantics
- implement real push constant recording
- add backend-private caching and reuse where required for scale:
  - pipeline layout caching
  - descriptor set layout reuse
  - descriptor allocator strategy
- validate pipeline-layout compatibility during bind

### Additional Vulkan-Specific Infrastructure

- remove swapchain-image-only assumptions from rendering entry points
- ensure offscreen textures can be transitioned, viewed, and rebound correctly
- make descriptor image layout selection state-aware rather than hardcoded to sampled-read layout

### Validation Target

A minimal Vulkan renderer should be able to perform:

1. offscreen scene pass with depth
2. sample the offscreen result
3. final present pass to the swapchain

### Deliverables

- Vulkan backend supports the full minimal renderer path

### Exit Criteria

- Vulkan can run the agreed minimal renderer demo without swapchain-only special casing

---

## Phase 6 - Align Metal To The Same Contract

### Objective

Ensure that the public graphics-path contract is genuinely cross-backend.

### Problem Being Solved

If Vulkan becomes the only backend that actually matches the intended model, the public API becomes theoretical again.

### Work

- match the Vulkan-side public semantics on Metal
- ensure resource binding, pass setup, attachment handling, and frame ownership all align
- support the same minimal renderer scenario on Metal
- compare asserts, limitations, and comments between backends and remove accidental divergence
- keep unsupported shared features consistently unsupported on both backends rather than "real on one side, fake on the other"

### Deliverables

- one renderer-side graphics path above the RHI for both backends

### Exit Criteria

- the same minimal renderer flow runs on Vulkan and Metal through the same public API

---

## Phase 7 - Cleanup, Documentation, And Validation

### Objective

Convert the repaired system into something maintainable.

### Work

- remove dead code and stale bring-up wording
- update milestone markers and transitional comments
- reconcile the implementation with the design documents
- add focused validation for the repaired behavior

### Recommended Validation Coverage

Unit-level or narrow tests:

- reflection conversion
- pipeline layout derivation
- parameter writer path resolution and binding behavior
- resource state tracker transition folding

Backend smoke coverage:

- clear and present
- depth rendering
- offscreen render target
- textured draw
- multi-set resource binding

Structural assertions:

- pipeline-layout compatibility at bind time
- frame-lifetime legality
- resource or view ownership invariants

### Deliverables

- cleaned docs
- reduced ambiguity for future contributors
- minimum automated or repeatable validation surface

### Exit Criteria

- the repaired render system is understandable and verifiable, not just functional

---

## 7. Recommended Execution Order

Recommended order:

1. Phase 0 - Freeze Scope And Success Criteria
2. Phase 1 - Make The Public RHI Honest
3. Phase 2 - Close The Shader And Resource Binding Model
4. Phase 3 - Redesign Frame And Pass Ownership
5. Phase 4 - Implement Real Frame Resources And Upload Infrastructure
6. Phase 5 - Complete Vulkan To Minimal-Renderer Level
7. Phase 6 - Align Metal To The Same Contract
8. Phase 7 - Cleanup, Documentation, And Validation

This order is deliberate:

- honesty must come before expansion
- semantics must come before renderer adoption
- ownership must come before multi-pass design
- frame and upload lifetime must come before serious content
- Vulkan completion should happen before final cross-backend lockstep validation

---

## 8. Milestones

### Milestone A - Honest RHI

Definition:

- no fake-success public paths
- no silent shell fallback for production-backend behavior
- unsupported features fail loudly and consistently

### Milestone B - Closed Resource Model

Definition:

- reflection, layout derivation, binding, constants, and push-constant metadata are coherent
- shader semantics are preserved through backend translation

### Milestone C - Renderer-Owned Frame Model

Definition:

- `Application` is no longer the owner of the implicit render pass
- a renderer-facing context explicitly owns pass recording

### Milestone D - Vulkan Minimal Renderer

Definition:

- Vulkan runs the agreed minimal renderer scenario with offscreen pass, depth, texture sampling, and present

### Milestone E - Cross-Backend Minimal Renderer

Definition:

- the same renderer-side flow runs on Vulkan and Metal through the same public contract

---

## 9. Risks And Likely Rework Points

### 9.1 Reflection model may need a richer neutral type system

If the current neutral reflection model cannot faithfully represent sampled texture versus storage texture, arrays, and push constants, some public or semi-public data structures may need to change.

### 9.2 Upload architecture may force API growth

If a real staging path is required, the current `CommandList` surface is too small. Some public RHI expansion is likely necessary.

### 9.3 Frame ownership changes will touch many call sites

Moving pass ownership out of `Application` will affect demos, renderer bootstrapping, and any code that currently assumes a globally accessible live command list.

### 9.4 Backend asymmetry pressure will remain

Vulkan will likely continue to act as the implementation reference. That is acceptable as long as Metal stays close enough that the public contract remains honest.

---

## 10. Immediate Next Steps

If execution starts now, the first concrete batch should be:

1. write and agree on the feature support matrix from Phase 0
2. remove fake-success paths for compute, push constants, and barriers
3. fix the texture-binding contract mismatch between `ShaderParameterWriter` and Vulkan
4. repair reflection semantics for storage textures, arrays, and push constants
5. move render-pass ownership out of `Application`

This first batch will not complete the whole plan, but it will remove the most dangerous architectural lies and make subsequent backend work much more stable.

---

## 11. Final Summary

The core idea of this remediation plan is simple:

- stop pretending the current scaffold is a renderer-capable RHI
- make the public API truthful
- make shader and resource semantics survive translation
- hand pass ownership back to the renderer
- replace single-frame demo assumptions with real frame and upload infrastructure
- finish Vulkan to a true minimal-renderer level
- keep Metal aligned to the same contract

Once those steps are complete, the project will have moved from a bring-up skeleton to a real rendering base on which a minimal renderer can be built without fighting foundational RHI debt every day.
