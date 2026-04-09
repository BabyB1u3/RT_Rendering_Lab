# RHI Backend — Metal

This document covers the Metal-specific implementation strategy for the RHI layer.

Metal should support two backend implementation modes:

- **direct slot binding**: individual `set*Buffer`, `set*Texture`, `set*SamplerState` calls per resource
- **argument-buffer-backed**: resources encoded into an `MTLBuffer`, then bound as a single argument buffer slot

Version 1 starts with direct slot binding and migrates selectively to argument buffers.

**The public RHI must not expose which mode is active.** Both are backend realizations of the same logical `ResourceSet`. `CommandList::bindResourceSet()` must behave identically to the caller regardless of which mode the Metal backend uses internally.

To prevent the direct-slot API shape from being permanently locked in, the Metal backend should maintain a backend-private binding plan per set index that encapsulates the chosen mode and its slot assignments. This plan is selected once at pipeline layout translation time and is opaque to the public layer:

```cpp
// Metal-backend-private — not part of the public RHI.
struct MetalSetBindingPlan
{
    enum class Mode { DirectSlot, ArgumentBuffer };
    Mode mode;
    // DirectSlot:      per-resource (stage, slotIndex) assignments
    // ArgumentBuffer:  argument buffer slot index + internal resource layout
};
```

Migrating a set from direct-slot to argument-buffer mode requires only replacing its `MetalSetBindingPlan` and the corresponding backend encode/bind path. No public API changes and no renderer-code changes should be necessary.

For the public RHI API being mapped, see [RHI.md](RHI.md). For shader compilation outputs and `MetalCodeFormat`, see [ShaderSystem.md](ShaderSystem.md) §3.2.

---

## 1. Required Metal objects

- `MTLBuffer`
- `MTLTexture`
- `MTLSamplerState`
- `MTLRenderPipelineState`
- `MTLComputePipelineState`
- `MTLArgumentEncoder` if using argument buffers
- `MTLRenderCommandEncoder`
- `MTLComputeCommandEncoder`

---

## 2. Pipeline layout translation

The neutral reflected layout must be translated into Metal-visible resource indices.

This translation should maintain separate index spaces for:

- vertex-stage buffers/textures/samplers
- fragment-stage buffers/textures/samplers
- compute-stage buffers/textures/samplers

If a `ParameterBlock<>` is represented as an argument buffer, then the set maps to:

- one argument buffer slot
- plus internal encoded resource references

---

## 3. ResourceSet translation: direct slot mode

In direct slot mode:

- `ResourceSet` stores CPU-side resource pointers
- `bindResourceSet()` expands set contents into multiple encoder calls:
  - `setVertexBuffer`
  - `setFragmentBuffer`
  - `setVertexTexture`
  - `setFragmentTexture`
  - `setVertexSamplerState`
  - `setFragmentSamplerState`

This is simplest for early implementation and debugging.

---

## 4. ResourceSet translation: argument buffer mode

In argument-buffer mode:

- a `ResourceSet` owns a Metal argument buffer
- dirty updates rewrite encoded resource references
- bind step becomes binding the argument buffer itself

This mode is especially suitable for:
- material sets
- scene sets
- grouped stable resource blocks

---

## 5. Recommended first-pass Metal strategy

For version 1:

- Frame set: direct slot binding
- Material set: direct slot binding first, optionally argument buffer later
- Object set: direct slot binding or tiny constant buffer path

This reduces complexity while keeping future migration to argument buffers possible.

**Migration boundary rule:** when upgrading a set to argument-buffer mode, the change must be entirely confined to the `MetalSetBindingPlan` for that set index and its corresponding backend encode/bind logic. If this change requires touching any public API or renderer-level code, the abstraction boundary has been drawn incorrectly.

---

## 6. Barrier model

Metal does not have explicit image layout transitions like Vulkan. The Metal backend implements `CommandList::textureBarrier()` / `bufferBarrier()` (RHI.md §11.2) using `MTLFence` or `MTLEvent` signaling between encoders, plus `MTLBlitCommandEncoder` or memory barriers where required.

In practice, Metal's tile-based GPU architecture makes many barriers no-ops or encoder boundaries. The backend may choose to defer actual barrier insertion until `MTLCommandEncoder` boundaries rather than emitting a Metal-level construct for every `textureBarrier()` call.

---

## 7. MSL compilation strategy

The Metal backend must decide at load time whether to compile MSL source or load a pre-compiled `.metallib`, based on `CompiledShaderBlob::metalCodeFormat` (see ShaderSystem.md §3.2).

#### MslSource mode (development)

- Call `[MTLDevice newLibraryWithSource:options:error:]` at runtime.
- Convenient for iteration and shader hot reload — no offline toolchain step required.
- Incurs a compilation delay on first load (can be seconds for complex shaders).
- **Do not ship to end users in this mode.**

#### Metallib mode (release / default for shipped builds)

- Call `[MTLDevice newLibraryWithData:error:]` with the pre-compiled `.metallib` bytes.
- Zero runtime compilation cost; App Store safe.
- Requires an offline build step using Apple's toolchain:
  ```
  xcrun metal   -c shader.metal -o shader.air
  xcrun metallib shader.air     -o shader.metallib
  ```
- The `.metallib` bytes are stored in `CompiledShaderBlob::code` and shipped with the application.

#### Default policy

The build pipeline should produce `Metallib` blobs by default. `MslSource` blobs are produced only in development / hot-reload configurations. The Metal backend selects the load path purely from `metalCodeFormat` — no other code path changes are needed.
