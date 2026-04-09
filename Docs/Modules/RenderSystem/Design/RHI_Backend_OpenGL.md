# RHI Backend — OpenGL

This document covers the OpenGL-specific implementation strategy for the RHI layer.

OpenGL is the **compatibility backend**. Its implementation strategy is to simulate modern explicit resource grouping on top of OpenGL's state machine. The public RHI must remain unchanged — OpenGL adapts to the abstraction, not the other way around.

For the public RHI API being mapped, see [RHI.md](RHI.md). For shader compilation outputs consumed by this backend, see [ShaderSystem.md](ShaderSystem.md).

---

## 1. Required OpenGL objects

- program object
- buffer objects
- texture objects
- sampler objects
- VAOs
- framebuffer objects
- backend-generated binding maps

---

## 2. Binding map generation

For each shader program, generate a backend-private OpenGL binding map:

```cpp
struct GLBindingMapEntry
{
    ResourceKind kind;
    uint32_t setIndex;
    uint32_t logicalBinding;

    uint32_t glBindingPoint = 0;
    uint32_t glTextureUnit  = 0;
};
```

This map translates engine logical bindings into OpenGL physical bindings:

- uniform buffers → UBO binding points
- storage buffers → SSBO binding points
- textures → texture units
- samplers → sampler units

---

## 3. ResourceSet application

In OpenGL, `bindResourceSet(setIndex, set)` should:

- inspect all resources in the set
- bind each to the GL physical binding location
- update current GL program state expectations

This is not a native descriptor-set concept, but a compatibility-layer emulation.

---

## 4. Constant data handling

For ordinary constant data inside a parameter block:

Preferred implementation:
- upload to backend-managed UBO
- bind UBO to the appropriate GL binding point

This is cleaner and closer to the Vulkan/Metal mental model than trying to explode everything into individual uniforms.

---

## 5. Push constants emulation

OpenGL has no native push constant equivalent. The OpenGL backend emulates `CommandList::pushConstants()` via a **backend-managed transient UBO** bound to a reserved binding point.

Implementation rules:

- Reserve one UBO binding point per program (e.g. binding point 0) exclusively for push constant data. This slot must not overlap with any user-visible `ResourceSet` binding.
- The backend maintains a small CPU-side buffer matching the full push constant range declared in `PipelineLayoutDesc`.
- On `pushConstants(stageMask, offset, size, data)`: copy `data` into the CPU buffer at `offset`.
- Before each draw call: upload the CPU buffer to the backing GL buffer object (`glNamedBufferSubData` or `glBufferSubData`), then bind it (`glBindBufferRange`).
- If no push constants are declared for the current pipeline, skip this entirely.

This is intentionally not equivalent to Vulkan/Metal push constants in performance terms. It is a compatibility emulation. Renderer code that relies on push constants for very-high-frequency per-draw data should be aware of this cost on the OpenGL path.

---

## 6. Barrier model

OpenGL has no explicit resource state model. The OpenGL backend implements `CommandList::textureBarrier()` / `bufferBarrier()` (RHI.md §11.2) using `glMemoryBarrier()` with appropriate barrier bits derived from the requested state transition.

> **Important**: these mappings are **synchronization approximations for visibility hazards**, not true equivalents of Vulkan image layouts, queue ownership transfers, or full resource-state transitions. OpenGL has no concept of image layout — a texture is always implicitly in a "usable" layout. The barrier bits below only ensure memory writes become visible to subsequent reads; they do not model attachment lifecycle or swapchain presentation the way Vulkan does.

| Transition target       | `glMemoryBarrier` bits                          |
|-------------------------|-------------------------------------------------|
| `ShaderRead`            | `GL_TEXTURE_FETCH_BARRIER_BIT`                  |
| `ShaderReadWrite`       | `GL_SHADER_IMAGE_ACCESS_BARRIER_BIT`            |
| `UniformRead`           | `GL_UNIFORM_BARRIER_BIT`                        |
| `StorageRead/Write`     | `GL_SHADER_STORAGE_BARRIER_BIT`                 |
| `CopySource/CopyDest`   | `GL_PIXEL_BUFFER_BARRIER_BIT` or `GL_BUFFER_UPDATE_BARRIER_BIT` |

`TextureState::Present`, `TextureState::RenderTarget`, and `BufferState::IndirectArgument` have no direct OpenGL equivalent; the backend may treat them as no-ops or omit the `glMemoryBarrier` call entirely for those transitions. Do not interpret the absence of a barrier as incorrect — on OpenGL, render target transitions are managed implicitly by the driver through framebuffer binding changes.

---

## 7. Vertex input

OpenGL must use a separate vertex input path:

- create VAO-compatible vertex layouts
- bind index buffer as part of VAO-related state
- never route vertex/index binding through `ResourceSet`

---

## 8. Support scope

OpenGL should support a controlled shader subset.

The architecture should not assume:
- parity with Vulkan reflection semantics in every advanced case
- unlimited feature coverage
- backend-identical behavior for complex resource models

The goal is stable compatibility, not feature leadership.
