# Metal Backend - Architecture and Implementation Plan

This document describes the design for adding a Metal rendering backend to RTRLab.
It covers the mapping from the existing RHI interfaces to Metal API concepts,
platform integration, build system changes, and the phased implementation plan.

**Prerequisites**: Shader & Material System doc (`shader_material_system.md`),
Resource Packaging doc (`resource_packaging.md`).

---

## 1. Goals and Constraints

### 1.1 Goals

- Implement all 9 RHI interfaces (`IGraphicsDevice`, `IRenderCommand`, `IShader`,
  `IVertexBuffer`, `IIndexBuffer`, `IVertexArray`, `ITexture2D`, `IFramebuffer`,
  `IRenderTarget`) against the Metal 3 API.
- Share the same Slang shader source - `slangc` compiles to MSL at build time.
- Coexist with the OpenGL backend - backend selection at application startup via
  `SetDevice()`, no compile-time `#ifdef` in application code.
- Run on macOS 14+ (Sonoma) with Apple Silicon and Intel GPUs.

### 1.2 Non-Goals (This Phase)

- iOS / iPadOS / visionOS support (future).
- Runtime shader compilation - all MSL is pre-compiled to `MTLLibrary` at load time.
- Mesh shaders, ray tracing, or Metal 3.2 features beyond what the RHI exposes.
- Vulkan backend (separate effort, see roadmap R5).

---

## 2. Platform Integration

### 2.1 Window & Surface

GLFW supports Metal via `glfwCreateWindow()` with no OpenGL hints, plus
a `CAMetalLayer` obtained through GLFW's native access API:

```
GLFW window (no GL context)
    │
    ▼
NSWindow → contentView.layer = CAMetalLayer
    │
    ▼
MTLDevice → CAMetalLayer.device
    │
    ▼
nextDrawable → MTLDrawable (per-frame backbuffer)
```

**Key changes to `Window.cpp`**:

| Current (OpenGL) | Metal |
|-------------------|-------|
| `glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API)` | `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` |
| `glfwMakeContextCurrent()` + Glad | Not needed |
| `glfwSwapBuffers()` | `[drawable present]` via `MTLCommandBuffer` |

The window creation path must be parameterized by the backend. Two approaches:

- **Option A - Build-time**: CMake option `GLAB_GRAPHICS_BACKEND` selects OpenGL
  or Metal. Only one backend is compiled. Simpler, avoids shipping unused code.
- **Option B - Runtime**: Window accepts a `GraphicsAPI` enum and configures GLFW
  hints accordingly. Both backends are compiled; selected at startup.

**Recommendation**: Start with **Option A** for simplicity. The `SetDevice()` pattern
already supports runtime switching at the API level; the window hint difference is
the only platform-specific fork.

### 2.2 Objective-C++ Interop

Metal is an Objective-C API. All Metal backend files must be compiled as
Objective-C++ (`.mm` extension or `/ObjC++` flag). This is isolated to
`src/graphics/metal/` - no Objective-C leaks into the rest of the codebase.

**Header convention**: Public headers (`.h`) use opaque `void*` or forward-declared
wrapper types so that non-Metal translation units never see `#import <Metal/Metal.h>`.

```cpp
// MetalGraphicsDevice.h - clean C++ header
class MetalGraphicsDevice : public IGraphicsDevice
{
    // ...
private:
    struct Impl;              // pImpl hides Obj-C types
    std::unique_ptr<Impl> m_Impl;
};
```

Implementation files (`.mm`) include Metal headers and define `Impl`.

---

## 3. Architecture Overview

### 3.1 Metal Object Model vs OpenGL

| Concept | OpenGL | Metal |
|---------|--------|-------|
| Device | Implicit (context) | `MTLDevice` |
| Command submission | Immediate (driver-threaded) | Explicit: `MTLCommandQueue` → `MTLCommandBuffer` → `MTLRenderCommandEncoder` |
| Shader | `glCreateProgram` + linked stages | `MTLLibrary` → `MTLFunction` → `MTLRenderPipelineState` |
| Vertex input | VAO (`glVertexArrayAttribFormat`) | `MTLVertexDescriptor` on pipeline state |
| Uniform data | `glProgramUniform*` / UBO | Argument buffers / `setVertexBytes` / `setFragmentBytes` |
| Framebuffer | FBO + texture attachments | `MTLRenderPassDescriptor` + texture attachments |
| Swap chain | `glfwSwapBuffers` | `CAMetalLayer.nextDrawable` → present |

### 3.2 Command Encoding Model

The biggest architectural difference is Metal's explicit command model.
OpenGL issues GPU commands immediately; Metal batches them:

```
Per Frame:
  MTLCommandBuffer = commandQueue.commandBuffer()
    │
    ├── MTLRenderCommandEncoder (one per render pass)
    │     ├── setRenderPipelineState(...)
    │     ├── setVertexBuffer(...)
    │     ├── setFragmentTexture(...)
    │     ├── drawIndexedPrimitives(...)
    │     └── endEncoding()
    │
    ├── (more encoders for shadow pass, post-process, etc.)
    │
    └── commit() + presentDrawable()
```

**Mapping to RHI**: The current RHI assumes immediate-mode semantics (e.g.,
`IRenderCommand::DrawIndexed()` draws immediately). For the Metal backend:

1. `MetalRenderCommand` internally manages the current `MTLCommandBuffer` and
   `MTLRenderCommandEncoder`.
2. A `BeginFrame()` / `EndFrame()` pair on the device (or render command) brackets
   each frame's command buffer lifecycle.
3. `IRenderTarget::Bind()` ends the current encoder (if any) and begins a new
   `MTLRenderCommandEncoder` with the target's `MTLRenderPassDescriptor`.
4. `DrawIndexed()` / `DrawArrays()` record draw calls on the current encoder.
5. `EndFrame()` calls `endEncoding()`, `presentDrawable()`, and `commit()`.

This maps cleanly to the existing frame loop:

```
Application::Run()
  ├── [BeginFrame - create command buffer, acquire drawable]
  ├── layer.OnUpdate()
  ├── layer.OnRender()
  │     ├── renderTarget->Bind()     → begin encoder
  │     ├── renderCommand->Clear()   → load action
  │     ├── shader->Bind()           → set pipeline state
  │     ├── vao->Bind()              → set vertex/index buffers
  │     ├── renderCommand->DrawIndexed() → record draw
  │     └── renderTarget->Unbind()   → end encoder
  └── [EndFrame - present, commit]
```

### 3.3 Pipeline State Management

Metal requires pre-compiled `MTLRenderPipelineState` objects that bake together:
- Vertex function + fragment function
- Vertex descriptor (attribute layout)
- Color attachment pixel formats
- Depth/stencil attachment pixel format
- Blend state

This is fundamentally different from OpenGL's mutable state machine. Strategy:

- **Lazy pipeline state creation**: `MetalShader` caches pipeline states keyed by
  `(vertexDescriptor, colorFormat, depthFormat, blendState)`.
- On `DrawIndexed()`, the current vertex layout + render target format + blend state
  form a key. If no cached PSO exists, create one.
- PSO cache is per-shader. In practice, the number of unique combinations is small
  (< 10 per shader), so the cache is cheap.

```cpp
struct PipelineStateKey
{
    MTLVertexDescriptor* vertexDescriptor;
    MTLPixelFormat colorFormat;
    MTLPixelFormat depthFormat;
    bool blendEnabled;
    // ... hash & equality
};

std::unordered_map<PipelineStateKey, id<MTLRenderPipelineState>> m_PipelineCache;
```

### 3.4 Resource Management & Triple Buffering

Metal works best with **triple-buffered** resources for data that changes every
frame (uniform buffers, dynamic vertex buffers):

```
Frame N:     GPU reads buffer[0]
Frame N+1:   GPU reads buffer[1], CPU writes buffer[0]
Frame N+2:   GPU reads buffer[2], CPU writes buffer[1]
```

Implementation:
- `MetalGraphicsDevice` maintains a frame index (0, 1, 2) and a semaphore
  (`dispatch_semaphore_t`, initial value 3) for CPU-GPU synchronization.
- Dynamic buffers (`BufferUsage::DynamicDraw`) are allocated as a single
  `MTLBuffer` with 3× the requested size, offset by `frameIndex * alignedSize`.
- Static buffers (`BufferUsage::StaticDraw`) are single-buffered (no per-frame copy).

---

## 4. Interface Implementation Mapping

### 4.1 `MetalGraphicsDevice` : `IGraphicsDevice`

The central factory. Owns the Metal device and command queue.

**State**:

| Member | Type | Purpose |
|--------|------|---------|
| `m_Device` | `id<MTLDevice>` | GPU device handle |
| `m_CommandQueue` | `id<MTLCommandQueue>` | Single queue for all submissions |
| `m_FrameIndex` | `uint32_t` | Current triple-buffer index (0–2) |
| `m_FrameSemaphore` | `dispatch_semaphore_t` | Limits in-flight frames to 3 |
| `m_RenderCommand` | `Ref<MetalRenderCommand>` | Singleton render command |
| `m_Layer` | `CAMetalLayer*` | Drawable source (set from window) |

**Initialization**:

```objc
m_Device = MTLCreateSystemDefaultDevice();
m_CommandQueue = [m_Device newCommandQueue];
m_FrameSemaphore = dispatch_semaphore_create(3);
```

**Frame lifecycle methods** (called from Application main loop):

```cpp
void BeginFrame();  // wait on semaphore, get command buffer, acquire drawable
void EndFrame();    // end encoder, present drawable, commit, signal semaphore
```

### 4.2 `MetalRenderCommand` : `IRenderCommand`

Wraps the current `MTLRenderCommandEncoder`. Most state-setting calls are deferred
to the encoder.

| RHI Method | Metal Equivalent |
|------------|------------------|
| `Init()` | No-op (state set per-encoder) |
| `BeginFrame()` | `commandQueue.commandBuffer()`, acquire `CAMetalDrawable` |
| `EndFrame()` | `endEncoding()`, `presentDrawable()`, `commit()` |
| `BeginRenderPass(target, desc)` | Create `MTLRenderCommandEncoder` from `MTLRenderPassDescriptor` built from `desc` |
| `EndRenderPass()` | `[encoder endEncoding]` |
| `SetPipelineState(state)` | Look up / create cached `MTLRenderPipelineState` + `MTLDepthStencilState` |
| `SetViewport(x, y, w, h)` | `[encoder setViewport:{x, y, w, h, 0, 1}]` |
| `SetTexture(slot, texture)` | `[encoder setFragmentTexture:atIndex:]` |
| `DrawIndexed(vao, count)` | `[encoder drawIndexedPrimitives:...]` |
| `DrawArrays(mode, first, count)` | `[encoder drawPrimitives:...]` |

**Depth/stencil state**: Metal uses immutable `MTLDepthStencilState` objects.
`MetalRenderCommand::SetPipelineState()` caches a small set keyed by
`(depthTestEnabled, depthWriteEnabled)` and switches between them.

### 4.3 `MetalShader` : `IShader`

Loads pre-compiled MSL from the shader directory, creates `MTLLibrary` and
`MTLFunction` objects, and manages pipeline state cache.

**State**:

| Member | Type | Purpose |
|--------|------|---------|
| `m_Library` | `id<MTLLibrary>` | Compiled shader library |
| `m_VertexFunction` | `id<MTLFunction>` | Vertex entry point |
| `m_FragmentFunction` | `id<MTLFunction>` | Fragment entry point |
| `m_Name` | `std::string` | Shader name (e.g., "ForwardLit") |
| `m_PipelineCache` | `map<Key, PSO>` | Cached pipeline states |
| `m_UniformBuffers` | `map<uint32_t, MTLBuffer*>` | Per-binding UBOs (triple-buffered) |

**Uniform handling**: The current RHI uses name-based setters (`SetMat4("u_Model", ...)`).
Metal has no runtime uniform reflection like `glGetUniformLocation`. Strategy:

- **Phase 1 (immediate)**: Use `setVertexBytes` / `setFragmentBytes` for small
  uniforms (< 4 KB). The `MetalShader` maintains a CPU-side staging buffer per
  stage. `SetMat4()` etc. write into this buffer at known offsets. On draw,
  the buffer is uploaded via `setVertexBytes`.
- **Phase 2 (shader material system evolution)**: Migrate to slot-based
  `SetUniformBlock(binding, data, size)` which maps directly to
  `setVertexBuffer(buffer, offset, index)`. This is the target architecture
  described in `shader_material_system.md` Phase B.

**Offset mapping**: Slang's reflection API (or a build-time metadata export) provides
the byte offset of each named uniform within its constant buffer. A JSON sidecar
per shader stores this mapping:

```json
{
  "ForwardLit": {
    "vertex": {
      "u_Model":      { "offset": 0,  "size": 64 },
      "u_ViewProj":   { "offset": 64, "size": 64 }
    },
    "fragment": {
      "u_LightPos":   { "offset": 0,  "size": 12 },
      "u_Albedo":     { "offset": 16, "size": 16 }
    }
  }
}
```

This sidecar is generated at build time by `slangc -dump-reflection` and consumed
by `MetalShader` at load time.

### 4.4 `MetalVertexArray` : `IVertexArray`

Metal has no VAO concept. `MetalVertexArray` is a **software-side descriptor**
that stores vertex/index buffer references and the `MTLVertexDescriptor`.

**State**:

| Member | Type | Purpose |
|--------|------|---------|
| `m_VertexDescriptor` | `MTLVertexDescriptor*` | Attribute format and layout |
| `m_VertexBuffers` | `vector<Ref<IVertexBuffer>>` | Bound vertex buffers |
| `m_IndexBuffer` | `Ref<IIndexBuffer>` | Bound index buffer |

**`Bind()` behavior**: Unlike OpenGL, `Bind()` does not issue GPU commands. Instead,
it registers this VAO as the "current" on the `MetalRenderCommand`.

The current RHI still passes an explicit VAO to `DrawIndexed(vao, count)`, so the
Metal implementation treats that parameter as authoritative when present and also
refreshes the "current VAO" cache from it. `DrawArrays()` uses the cached VAO when
one has been bound previously. In both cases, the render command:

1. Reads the active VAO's `MTLVertexDescriptor` to look up / create the PSO.
2. Calls `[encoder setVertexBuffer:offset:atIndex:]` for each vertex buffer.
3. Uses the VAO's index buffer for indexed draws.

**`AddVertexBuffer()` implementation**: Translates `BufferLayout` elements to
`MTLVertexAttributeDescriptor` entries:

| `ShaderDataType` | `MTLVertexFormat` |
|-------------------|-------------------|
| `Float` | `MTLVertexFormatFloat` |
| `Float2` | `MTLVertexFormatFloat2` |
| `Float3` | `MTLVertexFormatFloat3` |
| `Float4` | `MTLVertexFormatFloat4` |
| `Int` | `MTLVertexFormatInt` |
| `Int2` | `MTLVertexFormatInt2` |
| `Int3` | `MTLVertexFormatInt3` |
| `Int4` | `MTLVertexFormatInt4` |
| `Mat3` | 3 × `Float3` (expanded) |
| `Mat4` | 4 × `Float4` (expanded) |

### 4.5 `MetalVertexBuffer` : `IVertexBuffer`

Wraps a `MTLBuffer`.

| Usage | Allocation Strategy |
|-------|---------------------|
| `StaticDraw` | `newBufferWithBytes:length:options:` with `MTLResourceStorageModeManaged` (macOS) |
| `DynamicDraw` | Triple-buffered `MTLBuffer` with `MTLResourceStorageModeShared`, 3× size |
| `StreamDraw` | Same as Dynamic |

**`SetData(data, size, offset)`**: For shared-mode buffers, `memcpy` directly into
the mapped pointer. For managed-mode, additionally call `didModifyRange:`.

### 4.6 `MetalIndexBuffer` : `IIndexBuffer`

Wraps a `MTLBuffer` containing `uint32_t` indices. Always static (single-buffered,
managed storage). Stores `m_Count` for draw calls.

### 4.7 `MetalTexture2D` : `ITexture2D`

Wraps a `MTLTexture`.

**Format mapping**:

| `TextureFormat` | `MTLPixelFormat` |
|-----------------|------------------|
| `R8` | `MTLPixelFormatR8Unorm` |
| `RGB8` | `MTLPixelFormatRGBA8Unorm` (RGB8 not natively supported; pad to RGBA) |
| `RGBA8` | `MTLPixelFormatRGBA8Unorm` |
| `RedInteger` | `MTLPixelFormatR32Sint` |
| `Depth24Stencil8` | `MTLPixelFormatDepth32Float_Stencil8` (Metal lacks D24S8; use D32FS8) |

**Note**: Metal does not support `RGB8` natively. When `TextureFormat::RGB8` is
requested, allocate `RGBA8` and pad incoming data (insert alpha = 255 per pixel)
in `SetData()` and `CreateFromFile()`.

**Mipmap generation**: Use `MTLBlitCommandEncoder.generateMipmaps(for:)`.

**`Bind(slot)`**: No-op in the current RHI. Texture binding is driven by
`IRenderCommand::SetTexture(slot, texture)`, and the render command applies the
native Metal texture/sampler state before the draw call.

### 4.8 `MetalFramebuffer` : `IFramebuffer`

Wraps a set of `MTLTexture` objects (color + depth attachments) and produces a
`MTLRenderPassDescriptor` on `Bind()`.

**State**:

| Member | Type | Purpose |
|--------|------|---------|
| `m_ColorTextures` | `vector<Ref<MetalTexture2D>>` | Color attachments |
| `m_DepthTexture` | `Ref<MetalTexture2D>` | Depth/stencil attachment |
| `m_Spec` | `FramebufferSpecification` | Width, height, attachment specs |

**`Bind()`**: Constructs a `MTLRenderPassDescriptor`:

```objc
MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor new];
desc.colorAttachments[0].texture = m_ColorTextures[0]->GetNativeTexture();
desc.colorAttachments[0].loadAction = MTLLoadActionClear;
desc.colorAttachments[0].storeAction = MTLStoreActionStore;
desc.colorAttachments[0].clearColor = MTLClearColorMake(r, g, b, a);
desc.depthAttachment.texture = m_DepthTexture->GetNativeTexture();
// ...
```

Then hands this descriptor to `MetalRenderCommand` to begin a new encoder.

**`Resize(w, h)`**: Recreates all attachment textures at the new size.

**`ReadPixel()`**: Uses `MTLBlitCommandEncoder` to copy from GPU texture to a
staging `MTLBuffer`, then reads from the buffer. This requires a GPU sync
(command buffer completion wait).

### 4.9 `MetalRenderTarget` : `IRenderTarget`

Two modes, same as OpenGL. No `Bind()` / `Unbind()` - binding is handled by
`MetalRenderCommand::BeginRenderPass()`.

| Mode | `GetFramebuffer()` | `BeginRenderPass` behavior |
|------|--------------------|-----------------------------|
| Back buffer | returns `nullptr` | Uses current `CAMetalDrawable`'s texture |
| Framebuffer | returns the FBO | Uses the FBO's attachment textures |

`MetalRenderCommand::BeginRenderPass()` builds `MTLRenderPassDescriptor` from
the `RenderPassDescriptor` struct and the target's attachments:

```objc
// Back buffer example
desc.colorAttachments[0].texture    = currentDrawable.texture;
desc.colorAttachments[0].loadAction = rpDesc.ColorLoadAction == LoadAction::Clear
                                        ? MTLLoadActionClear : MTLLoadActionLoad;
desc.colorAttachments[0].storeAction = MTLStoreActionStore;
desc.colorAttachments[0].clearColor  = MTLClearColorMake(r, g, b, a);
```

---

## 5. Shader Pipeline Integration

### 5.1 Build-Time Compilation

Already supported in `cmake/CompileShaders.cmake`:

```cmake
set(GLAB_SHADER_TARGET_METAL ON)
```

Output: `build/shaders/metal/{ShaderName}.metal` - a single `.metal` file
containing both vertex and fragment functions.

### 5.2 Runtime Loading

```
build/shaders/metal/ForwardLit.metal
    │
    ▼
NSString* source = [NSString stringWithContentsOfFile:...]
    │
    ▼
id<MTLLibrary> lib = [device newLibraryWithSource:source options:nil error:&err]
    │
    ▼
id<MTLFunction> vertexFunc   = [lib newFunctionWithName:@"vertexMain"]
id<MTLFunction> fragmentFunc = [lib newFunctionWithName:@"fragmentMain"]
```

**Entry point naming**: Slang generates `vertexMain` and `fragmentMain` by default.
Verify this matches the Slang Metal output and adjust if needed.

**Precompilation (optional optimization)**: Use `xcrun -sdk macosx metal` and
`metallib` at build time to produce `.metallib` binaries, loaded via
`newLibraryWithURL:`. This skips runtime compilation. Can be added as a
CMake post-compile step.

### 5.3 Reflection Sidecar

For name-based uniform setters to work, a build-time step extracts reflection:

```cmake
# Added to glab_compile_shaders() for Metal target
add_custom_command(
    OUTPUT ${SHADER_NAME}.reflect.json
    COMMAND slangc -target metal -dump-ir-ids -reflection-json ...
)
```

Alternatively, use Slang's C++ reflection API at load time (linked into the
application). The JSON sidecar is simpler and avoids a Slang runtime dependency.

---

## 6. Build System Changes

### 6.1 CMake Structure

```cmake
# src/graphics/CMakeLists.txt (or src/CMakeLists.txt)

option(GLAB_BACKEND_OPENGL "Build OpenGL backend" ON)
option(GLAB_BACKEND_METAL  "Build Metal backend"  OFF)

if(GLAB_BACKEND_OPENGL)
    target_sources(RTRLab PRIVATE
        graphics/opengl/GLGraphicsDevice.cpp
        # ... all GL files
    )
    target_link_libraries(RTRLab PRIVATE OpenGL::GL glad)
endif()

if(GLAB_BACKEND_METAL)
    target_sources(RTRLab PRIVATE
        graphics/metal/MetalGraphicsDevice.mm
        graphics/metal/MetalRenderCommand.mm
        graphics/metal/MetalShader.mm
        graphics/metal/MetalVertexArray.mm
        graphics/metal/MetalVertexBuffer.mm
        graphics/metal/MetalIndexBuffer.mm
        graphics/metal/MetalTexture2D.mm
        graphics/metal/MetalFramebuffer.mm
        graphics/metal/MetalRenderTarget.mm
    )
    target_link_libraries(RTRLab PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework AppKit"
    )
    set(GLAB_SHADER_TARGET_METAL ON)
endif()
```

### 6.2 File Layout

```
src/graphics/
  interface/           ← existing, unchanged
  opengl/              ← existing, unchanged
  metal/               ← NEW
    MetalCast.h                  ← AsNative<> helper for Obj-C casts
    MetalGraphicsDevice.h/.mm
    MetalRenderCommand.h/.mm
    MetalShader.h/.mm
    MetalVertexArray.h/.mm
    MetalVertexBuffer.h/.mm
    MetalIndexBuffer.h/.mm
    MetalTexture2D.h/.mm
    MetalFramebuffer.h/.mm
    MetalRenderTarget.h/.mm
    MetalTypes.h                 ← Format/enum conversion helpers
  GraphicsDevice.h/.cpp          ← existing, unchanged
```

### 6.3 Window Backend Selection

In `Application.cpp`, the device creation becomes backend-aware:

```cpp
#if defined(GLAB_BACKEND_METAL)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // ... create window ...
    SetDevice(CreateRef<MetalGraphicsDevice>(window->GetNativeHandle()));
#else
    // ... existing OpenGL path ...
    SetDevice(CreateRef<GLGraphicsDevice>());
#endif
```

Or, extract this into a factory function in `GraphicsDevice.cpp` that reads the
CMake-defined backend.

---

## 7. Implementation Phases

### Phase 1 - Skeleton & Triangle (1–2 weeks)

**Goal**: Metal window with a colored triangle on screen.

| Task | Details |
|------|---------|
| Create `src/graphics/metal/` directory | 10 files (`.h` + `.mm`) |
| `MetalGraphicsDevice` | `MTLDevice`, `MTLCommandQueue`, `CAMetalLayer` setup |
| `MetalRenderCommand` | `BeginFrame/EndFrame`, `BeginRenderPass/EndRenderPass`, `SetPipelineState`, `SetTexture`, `DrawArrays` |
| `MetalVertexBuffer` | Static buffer creation and data upload |
| `MetalIndexBuffer` | Static index buffer |
| `MetalVertexArray` | `MTLVertexDescriptor` construction from `BufferLayout` |
| `MetalShader` (minimal) | Load `.metal` source → `MTLLibrary` → `MTLFunction`, hardcoded PSO |
| Window integration | `GLFW_NO_API` path, `CAMetalLayer` from `NSWindow` |
| CMake | `GLAB_BACKEND_METAL` option, `.mm` compilation, framework linking |

**Validation**: Hardcoded triangle renders with correct vertex colors.

### Phase 2 - Uniforms & Transforms (1 week)

**Goal**: Rotating 3D cube with camera.

| Task | Details |
|------|---------|
| `MetalShader` uniform setters | CPU staging buffer + `setVertexBytes/setFragmentBytes` |
| Reflection sidecar loading | Parse JSON for name → offset mapping |
| `MetalShader` pipeline cache | Key by vertex descriptor + pixel format + blend state |
| Dynamic vertex buffers | Triple-buffered `DynamicDraw` path |
| Depth state | `MTLDepthStencilState` creation and caching |

**Validation**: Existing `ShadowMapping` demo loads and renders geometry (no shadows yet).

### Phase 3 - Textures & Framebuffers (1–2 weeks)

**Goal**: Full feature parity with OpenGL backend for existing demos.

| Task | Details |
|------|---------|
| `MetalTexture2D` | Create, load from file (stb_image), format conversion, mipmap generation |
| Texture binding | Track active textures, bind on encoder before draw |
| `MetalFramebuffer` | Multi-attachment FBO, resize, read pixel |
| `MetalRenderTarget` | Back buffer + FBO wrapper |
| `SetUniformBlock` | Metal buffer-based uniform blocks (triple-buffered) |
| Blend, cull, depth state | Complete state management |

**Validation**: `ShadowMapping` and `MaterialPlayground` demos run correctly.

### Phase 4 - Polish & Integration (1 week)

**Goal**: Production-ready Metal backend.

| Task | Details |
|------|---------|
| ImGui Metal renderer | Integrate `imgui_impl_metal` (Dear ImGui ships this) |
| Error handling | `MTLCommandBuffer` error/status callbacks, GPU validation layer |
| Performance | GPU frame capture validation, triple-buffer tuning |
| CI | macOS build job with Metal backend |
| `.metallib` precompilation | Optional CMake step for faster shader loading |

**Validation**: All demos run on macOS with Metal validation layer enabled,
no errors, stable frame times.

---

## 8. RHI Interface Evolution (Completed)

The following interface changes have been implemented to support Metal/Vulkan backends.
These changes are backwards-compatible - the OpenGL backend continues to work with
the same semantics, just expressed through the new API.

### 8.1 Frame Lifecycle - `BeginFrame()` / `EndFrame()`

**File**: `IRenderCommand.h`

Added virtual methods with default empty implementations:

```cpp
virtual void BeginFrame() {}   // Metal: create MTLCommandBuffer, acquire drawable
virtual void EndFrame() {}     // Metal: commit + present
```

**Application main loop** (`Application.cpp`) now brackets the render phase:

```cpp
RenderCommand::BeginFrame();
// ... layer updates, rendering, ImGui ...
RenderCommand::EndFrame();
```

OpenGL: no-op. Metal/Vulkan: command buffer lifecycle.

### 8.2 Render Pass Descriptors - `BeginRenderPass()` / `EndRenderPass()`

**Files**: `RenderTypes.h`, `IRenderCommand.h`

Replaced the implicit `target->Bind()` + `SetClearColor()` + `Clear()` pattern with
an explicit render pass model:

```cpp
struct RenderPassDescriptor {
    LoadAction  ColorLoadAction;   // Clear, Load, or DontCare
    StoreAction ColorStoreAction;
    glm::vec4   ClearColor;
    LoadAction  DepthLoadAction;
    StoreAction DepthStoreAction;
    float       ClearDepth;
    // ... stencil
};

virtual void BeginRenderPass(const Ref<IRenderTarget>& target,
                             const RenderPassDescriptor& desc) = 0;
virtual void EndRenderPass() = 0;
```

**IRenderTarget** no longer has `Bind()` / `Unbind()`. Instead, it exposes
`GetFramebuffer()` so backends can access the underlying FBO internally.

Metal maps directly to `MTLRenderPassDescriptor`. Vulkan maps to
`VkRenderPassBeginInfo`. OpenGL translates to `glBindFramebuffer` +
`glClearColor` + `glClear`.

### 8.3 Pipeline State Object - `SetPipelineState()`

**Files**: `RenderTypes.h`, `IRenderCommand.h`

Replaced individual `EnableDepthTest()`, `EnableBlend()`, `EnableCullFace()`,
`SetCullFace()` calls with a single immutable descriptor:

```cpp
struct PipelineState {
    bool DepthTestEnabled;
    bool DepthWriteEnabled;
    bool BlendEnabled;
    bool CullFaceEnabled;
    bool CullFront;
};

virtual void SetPipelineState(const PipelineState& state) = 0;
```

Metal/Vulkan bake this into a cached pipeline state object (keyed by shader +
vertex layout + state). OpenGL translates to `glEnable` / `glDisable` calls.

### 8.4 Explicit Texture Binding - `SetTexture()`

**File**: `IRenderCommand.h`

Replaced `texture->Bind(slot)` calls in render passes with:

```cpp
virtual void SetTexture(uint32_t slot, const Ref<ITexture2D>& texture) = 0;
```

Render passes now use `RenderCommand::SetTexture(slot, texture)` instead of
calling `texture->Bind(slot)` directly. This gives the backend full control
over how textures are bound to the GPU pipeline.

Metal: `[encoder setFragmentTexture:atIndex:]`.
Vulkan: descriptor set update.
OpenGL: `glBindTextureUnit(slot, textureID)`.

### 8.5 Render Target Caching

Render targets are now created once and cached as class members, not per-frame:

- `ShadowPass` owns `m_RenderTarget` (created in constructor)
- `ForwardPass` owns `m_RenderTarget` (created in constructor)
- `SceneRenderer` owns `m_BackBufferTarget` (resized on viewport change)
- `FrameResources` no longer carries `Ref<IRenderTarget>` - it carries
  `Ref<ITexture2D>` output textures and a shared `BackBuffer` target

### 8.6 Name-Based Uniforms → Slot-Based (Future)

As noted in `shader_material_system.md`, the long-term target is slot-based
`SetUniformBlock(binding, data, size)`. The Metal backend implements name-based
setters via reflection as a bridge, but the slot-based path should be prioritized
as the next material system evolution - it maps directly to Metal's buffer
binding model and eliminates the reflection sidecar.

---

## 9. Risk & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Slang MSL output has unexpected entry point names or binding layout | Shader loading fails | Test `slangc` Metal output early (Phase 1); add a shader compatibility test |
| `RGB8` texture padding adds overhead for large textures | Memory waste (33%) for RGB textures | Convert to RGBA at asset cooking time (Phase B of resource packaging), not at runtime |
| Pipeline state explosion (many PSO combinations) | Hitches on first draw with new state | Pre-warm common PSOs at startup; in practice, demo PSO count is < 20 |
| `ReadPixel` requires GPU sync | Frame stall on picking | Acceptable for editor tooling; use async readback for production use cases |
| GLFW Metal support quirks | Window/resize issues | GLFW 3.4+ has stable Metal support; test resize and fullscreen paths early |
| Objective-C++ compilation slows build | Longer build times | Isolated to `metal/` directory; PCH covers C++ headers; `.mm` files are few |

---

## 10. File Checklist

New files to create:

```
src/graphics/metal/
  MetalCast.h
  MetalTypes.h
  MetalGraphicsDevice.h
  MetalGraphicsDevice.mm
  MetalRenderCommand.h
  MetalRenderCommand.mm
  MetalShader.h
  MetalShader.mm
  MetalVertexArray.h
  MetalVertexArray.mm
  MetalVertexBuffer.h
  MetalVertexBuffer.mm
  MetalIndexBuffer.h
  MetalIndexBuffer.mm
  MetalTexture2D.h
  MetalTexture2D.mm
  MetalFramebuffer.h
  MetalFramebuffer.mm
  MetalRenderTarget.h
  MetalRenderTarget.mm
```

Files to modify:

```
CMakeLists.txt (or src/CMakeLists.txt)  - add GLAB_BACKEND_METAL option
cmake/CompileShaders.cmake              - enable METAL target when backend is Metal
src/core/app/Window.cpp                 - GLFW_NO_API path for Metal
src/core/app/Application.cpp            - MetalGraphicsDevice creation path (BeginFrame/EndFrame already added)
```

Files already modified (RHI evolution, §8):

```
src/graphics/RenderTypes.h              - NEW: RenderPassDescriptor, PipelineState, LoadAction, StoreAction
src/graphics/interface/IRenderCommand.h - BeginFrame/EndFrame, BeginRenderPass/EndRenderPass, SetPipelineState, SetTexture
src/graphics/interface/IRenderTarget.h  - removed Bind/Unbind, added GetFramebuffer()
src/graphics/RenderCommand.h/.cpp       - static wrapper updated to match new interface
src/graphics/opengl/GLRenderCommand.h/.cpp  - OpenGL implementation of new methods
src/graphics/opengl/GLRenderTarget.h/.cpp   - removed Bind/Unbind override, kept GL-internal logic
src/renderer/RenderContext.h            - FrameResources simplified (textures + BackBuffer only)
src/renderer/SceneRenderer.h/.cpp       - cached m_BackBufferTarget
src/renderer/passes/ShadowPass.h/.cpp   - uses BeginRenderPass + PipelineState + cached target
src/renderer/passes/ForwardPass.h/.cpp  - uses BeginRenderPass + PipelineState + SetTexture + cached target
src/renderer/passes/TexturePreviewPass.cpp - uses BeginRenderPass + PipelineState + SetTexture
tests/integration/TestRenderTarget.cpp  - updated for new API
```
