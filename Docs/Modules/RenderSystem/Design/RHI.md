# RHI Design
## Slang + C++ Multi-Backend Renderer — RHI Public API Contract
## Target Backends: OpenGL / Metal / Vulkan

This document defines the **public RHI API contract** — the backend-neutral interface that renderer code uses to allocate resources, create pipelines, record commands, and present frames. It is the primary reference for anyone writing rendering code above the RHI layer.

Related documents:
- [ShaderSystem.md](ShaderSystem.md) — Slang shader authoring, neutral reflection model, shader compilation, and the parameter writer
- [RHI_Backend_Vulkan.md](RHI_Backend_Vulkan.md) — Vulkan implementation strategy
- [RHI_Backend_Metal.md](RHI_Backend_Metal.md) — Metal implementation strategy
- [RHI_Backend_OpenGL.md](RHI_Backend_OpenGL.md) — OpenGL implementation strategy

---

## 1. Document Purpose

This document defines a practical rendering backend abstraction for a C++ engine that uses **Slang** as the shader language and supports the following graphics backends simultaneously:

- **Vulkan**
- **Metal**
- **OpenGL**

The design goal is to create a rendering backend abstraction that is:

- modern enough to map naturally to Vulkan
- flexible enough to map cleanly to Metal
- compatible enough to support OpenGL as a secondary backend
- driven by **Slang reflection**, instead of duplicated hand-maintained shader binding metadata
- suitable as a long-term foundation for:
  - material systems
  - renderer passes
  - hot reload
  - future render graph integration
  - future compute support

This document focuses on the **RHI / resource binding / command recording** level, not on higher-level renderer features such as scene management or ECS.

---

## 2. Core Design Principles

### 2.1 Vulkan-style public model, not OpenGL-style public model

The public abstraction should not be based on implicit global state like traditional OpenGL.

Do **not** design the public API around ideas like:

- "bind texture unit 3"
- "bind UBO slot 2"
- "set current shader"
- "mutate global context state and then draw"

Instead, the public API should be built around explicit objects:

- `ShaderProgram`
- `PipelineLayout`
- `ResourceSet`
- `GraphicsPipeline`
- `ComputePipeline`
- `CommandList`

This gives us:

- a natural mapping to Vulkan
- a clean mapping to Metal
- a compatibility translation layer for OpenGL

OpenGL should be treated as an implementation backend, **not** as the architectural reference model.

---

### 2.2 Reflection is the single source of truth

All shader-visible parameter layout should come from **Slang reflection**.

The engine should **not** maintain a second manually-authored binding database in C++.

That means:

- no hand-maintained `binding = 5` tables in engine code
- no duplicated struct layout definitions in both shader and C++
- no duplicated texture/sampler register maps

Instead:

- Slang compiles shader modules
- Slang reflection produces layout metadata
- the engine converts that metadata into an internal neutral layout representation
- all backends consume that neutral layout representation

This is one of the central architectural decisions. See [ShaderSystem.md](ShaderSystem.md) for full details.

---

### 2.3 Separate resource binding from vertex input binding

Vertex/index buffers must **not** be treated as ordinary shader resources.

They are a separate concept in all three backends:

- **Vulkan**: vertex/index buffers are bound separately from descriptor sets
- **Metal**: vertex input is not the same abstraction as argument buffers or general resource sets
- **OpenGL**: index buffer binding is part of VAO state, and vertex attribute layout has its own system

Therefore, the design must keep these two systems separate:

#### Shader resource binding
- uniform data
- uniform buffers
- storage buffers
- sampled textures
- storage textures
- samplers

#### Vertex input binding
- vertex layout
- vertex buffers
- index buffer
- instance step mode

This separation is mandatory.

---

### 2.4 OpenGL is a compatibility backend, not a feature-defining backend

Because Slang's Vulkan/SPIR-V and Metal paths are stronger than its OpenGL/GLSL path, this architecture must assume:

- Vulkan and Metal are the primary-quality targets
- OpenGL support is restricted to a well-controlled shader subset
- advanced shader system decisions must not be limited by OpenGL's weakest points

That means:

- the public abstraction remains modern
- OpenGL adapts to the abstraction
- the abstraction does **not** collapse down to OpenGL-era state machine semantics

---

### 2.5 Prefer logical resource grouping

Shader parameters should be grouped by logical responsibility and update frequency, not by low-level backend details.

Recommended grouping:

- **Set 0**: Frame / Scene parameters
- **Set 1**: Material parameters
- **Set 2**: Object / Instance parameters
- **Push constants**: very small and highly dynamic parameters

This layout aligns well with:

- Vulkan descriptor-set usage
- Metal argument-buffer grouping
- OpenGL compatibility-layer batch application

---

## 3. High-Level Architecture

The engine is divided into the following major subsystems:

```text
Engine/
├── RHI/
│   ├── Common/
│   ├── Vulkan/
│   ├── Metal/
│   └── OpenGL/
│
├── ShaderSystem/
│   ├── SlangCompiler
│   ├── SlangModuleCache
│   ├── ShaderReflection
│   ├── ShaderParameterWriter
│   └── ShaderHotReload
│
├── Renderer/
│   ├── MaterialSystem
│   ├── Passes
│   ├── FrameGlobals
│   └── MeshDraw
│
└── Shaders/
    ├── scene.slang
    ├── material.slang
    ├── object.slang
    ├── pbr.slang
    └── fullscreen.slang
```

---

## 4. Major Subsystems

### 4.1 RHI Layer

The RHI is responsible for the backend-independent rendering API.

It defines:

- abstract device interfaces
- resource types
- pipeline types
- command recording interfaces
- swapchain interfaces
- synchronization primitives

It does **not** know Slang internals directly. It only consumes the neutral reflection/layout data produced by the shader system.

---

### 4.2 Shader System

The shader system handles Slang compilation, reflection extraction, and CPU-side parameter writing. It is the bridge between Slang and the RHI.

See [ShaderSystem.md](ShaderSystem.md) for full details.

---

### 4.3 Renderer Layer

The renderer sits above the RHI and shader system.

It is responsible for:

- creating frame-level resource sets
- creating material instances
- creating object-instance bindings
- selecting pipelines
- recording draw/dispatch commands

The renderer should not care whether the backend is Vulkan, Metal, or OpenGL.

---

## 5. RHI Public Object Model

The RHI exposes the following major object categories:

- Device
- Buffer
- Texture
- TextureView
- Sampler
- ShaderProgram
- PipelineLayout
- ResourceSet
- GraphicsPipeline
- ComputePipeline
- VertexInputLayout
- Swapchain
- CommandList

---

## 6. Resource Model

### 6.1 Format

`Format` describes the memory layout and interpretation of pixel data used by textures, render targets, vertex attributes, and pipeline attachment descriptors.

```cpp
enum class Format
{
    Unknown,

    // Color — 8-bit normalized
    R8_UNORM,
    RG8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,        // common swapchain format
    BGRA8_SRGB,

    // Color — 16-bit float
    R16F,
    RG16F,
    RGBA16F,

    // Color — 32-bit float
    R32F,
    RG32F,
    RGBA32F,

    // Integer
    R32_UINT,

    // Depth / stencil
    D16_UNORM,
    D32_SFLOAT,
    D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT,
};
```

---

### 6.2 Buffer

```cpp
enum class BufferUsage : uint32_t
{
    Vertex      = 1 << 0,
    Index       = 1 << 1,
    Uniform     = 1 << 2,
    Storage     = 1 << 3,
    CopySrc     = 1 << 4,
    CopyDst     = 1 << 5,
    Indirect    = 1 << 6,
};

enum class MemoryUsage
{
    GpuOnly,
    CpuToGpu,
    GpuToCpu,
};

struct BufferDesc
{
    uint64_t    size = 0;
    BufferUsage usageMask = BufferUsage(0);
    MemoryUsage memoryUsage = MemoryUsage::GpuOnly;
    const char* debugName = nullptr;
};
```

Responsibilities:

- GPU buffer allocation
- upload/readback usage support depending on memory mode
- backend resource ownership
- optionally map/unmap where appropriate

---

### 6.3 Texture

```cpp
enum class TextureType
{
    Tex2D,
    Tex2DArray,
    Tex3D,
    Cube,
};

enum class TextureUsage : uint32_t
{
    Sampled      = 1 << 0,
    Storage      = 1 << 1,
    RenderTarget = 1 << 2,
    DepthStencil = 1 << 3,
    CopySrc      = 1 << 4,
    CopyDst      = 1 << 5,
};

struct Extent3D
{
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
};

struct TextureDesc
{
    TextureType type;
    Format format;
    Extent3D extent;
    uint32_t     mipLevels = 1;
    uint32_t     arrayLayers = 1;
    TextureUsage usageMask = TextureUsage(0);
    const char*  debugName = nullptr;
};
```

---

### 6.4 Sampler

```cpp
enum class FilterMode
{
    Nearest,
    Linear,
};

enum class MipFilterMode
{
    None,     // no mip interpolation; use base level only
    Nearest,  // snap to nearest mip level
    Linear,   // linear interpolation between mip levels (trilinear)
};

enum class AddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

struct SamplerDesc
{
    FilterMode minFilter;
    FilterMode magFilter;
    MipFilterMode mipFilter;
    AddressMode addressU;
    AddressMode addressV;
    AddressMode addressW;
    float minLod = 0.0f;
    float maxLod = FLT_MAX;
    float mipLodBias = 0.0f;
    bool anisotropyEnable = false;
    float maxAnisotropy = 1.0f;
};
```

---

### 6.5 TextureView

A `TextureView` is a typed, ranged window into a `Texture` that can be bound to a shader stage or used as a render / depth attachment. It does not own the underlying texture memory.

```cpp
enum class TextureAspect : uint32_t
{
    Color   = 1 << 0,
    Depth   = 1 << 1,
    Stencil = 1 << 2,
};

struct TextureViewDesc
{
    TextureType   type            = TextureType::Tex2D;
    Format        format          = Format::Unknown;   // Unknown = inherit format from texture
    TextureAspect aspect          = TextureAspect::Color;
    uint32_t      baseMipLevel    = 0;
    uint32_t      mipLevelCount   = 1;   // 0 = all remaining mip levels
    uint32_t      baseArrayLayer  = 0;
    uint32_t      arrayLayerCount = 1;
};
```

---

### 6.6 Swapchain

A `Swapchain` manages a platform-specific sequence of presentable images and drives the acquire → render → present lifecycle. It is the only RHI object whose images are **not** owned by the caller — swapchain images are owned by the swapchain itself.

```cpp
struct SwapchainDesc
{
    uint32_t width;
    uint32_t height;
    Format   format     = Format::BGRA8_UNORM;  // requested; actual format may differ
    uint32_t imageCount = 2;                     // requested; actual count may differ
    bool     vsync      = true;
};

enum class NativeWindowSystem
{
    Win32,
    Cocoa,
    Xlib,
    Xcb,
    Wayland,
};

struct NativeWindowHandle
{
    NativeWindowSystem system;

    // Platform-native top-level window or surface handle:
    // - Win32:   HWND
    // - Cocoa:   NSWindow*
    // - Xlib:    X11 Window value cast to uintptr_t
    // - Xcb:     xcb_window_t value cast to uintptr_t
    // - Wayland: wl_surface*
    uintptr_t window = 0;

    // Optional companion native object:
    // - Xlib:    Display*
    // - Xcb:     xcb_connection_t*
    // - Wayland: wl_display*
    // - Win32 / Cocoa: null
    void* display = nullptr;

    // Optional presentation layer object when required by a backend:
    // - Metal:   CAMetalLayer*
    // - Vulkan on Apple platforms: CAMetalLayer*
    // - others:  null
    void* layer = nullptr;
};

class Swapchain
{
public:
    // Acquire the next presentable image. Returns the image index for this frame.
    // Internally signals the backend semaphore/fence for GPU synchronisation.
    uint32_t acquireNextImage();

    // Retrieve the image and a default full-image view for the given index.
    // Lifetime is tied to the Swapchain — do not delete these pointers.
    Texture*     getImage    (uint32_t imageIndex) const;
    TextureView* getImageView(uint32_t imageIndex) const;

    // Present the rendered image. Must be called after Device::submit() for
    // the command list that rendered into imageIndex.
    void present(uint32_t imageIndex);

    // Recreate internal resources after a window resize.
    // Invalidates all existing image/view pointers — call getImage/getImageView again.
    void resize(uint32_t newWidth, uint32_t newHeight);

    // Query current state (may differ from SwapchainDesc requests).
    uint32_t width()      const;
    uint32_t height()     const;
    Format   format()     const;
    uint32_t imageCount() const;
};
```

**Image ownership rule**: `getImage()` / `getImageView()` return non-owning pointers. The swapchain owns all its images and views; callers must not delete them or wrap them in `Scope<T>`.

**Native window handle**: `Device::createSwapchain()` takes a strongly typed `NativeWindowHandle`, not a `GLFWwindow*` and not a raw `void*`.

This is an intentional abstraction boundary:

- the **windowing layer** may use GLFW internally
- the **RHI public API** should remain independent from any particular window library
- the windowing layer is responsible for extracting platform-native handles from `GLFWwindow*` and packaging them into `NativeWindowHandle`

This keeps GLFW from leaking into the public renderer API and makes future platform-layer changes low-friction.

---

## 7. Pipeline Layout Model

A `PipelineLayout` defines the shader-visible resource interface in a backend-neutral way.

It should be derived from shader reflection via `ShaderProgram::derivePipelineLayoutDesc()`, not hand-authored. `ShaderProgram` and `PipelineLayout` are collaborators — `ShaderProgram` does not own the layout.

---

### 7.1 Binding kinds

`ResourceKind` describes how a resource is actually bound at the backend level — this is the **RHI binding model**, distinct from the logical `ReflectedTypeKind` used in the reflection model (ShaderSystem.md §2.1).

Key mapping rule: logical constant/struct data (`ReflectedTypeKind::ConstantData`) always maps to `UniformBuffer` at this level. The engine uploads the CPU-side constants blob into a backend-managed uniform buffer. Push constants are tracked separately via `PushConstantRangeDesc` and do not appear here.

```cpp
enum class ResourceKind
{
    UniformBuffer,    // constant / parameter-block data backed by a uniform buffer
    StorageBuffer,    // read/write structured or raw buffer
    SampledTexture,   // texture sampled in a shader stage
    StorageTexture,   // read/write storage image
    Sampler,          // sampler state object
};
```

---

### 7.2 Binding info

```cpp
struct BindingInfo
{
    std::string  name;
    uint32_t     setIndex = 0;
    uint32_t     binding = 0;
    ResourceKind kind;
    uint32_t     arrayCount = 1;
    ShaderStage  stageMask = ShaderStage(0);  // ShaderStage defined in ShaderSystem.md §3.2
};
```

---

### 7.3 PipelineLayoutDesc

```cpp
struct PipelineLayoutDesc
{
    std::vector<BindingInfo> bindings;
    std::vector<PushConstantRangeDesc> pushConstants;  // PushConstantRangeDesc defined in ShaderSystem.md §2.3
};
```

---

### 7.4 PipelineLayout object

`PipelineLayout` should:

- represent the neutral shader resource interface
- own per-backend translated layout objects
- provide set-wise access to logical layout information

`ResourceSet` instances are created exclusively via `Device::createResourceSet()`. `PipelineLayout` does not act as a factory — this keeps all object creation on a single point and removes ownership ambiguity.

```cpp
class PipelineLayout
{
public:
    const PipelineLayoutDesc& getDesc() const;
};
```

---

## 8. ResourceSet Design

A `ResourceSet` represents one logical shader parameter group instance, such as:

- one frame set
- one material set
- one object set

It is the runtime instance of the shader-defined parameter block.

**Two-layer model:** `ResourceSet` is the logical shader-parameter container. Backend binding objects derived from it — Vulkan `VkDescriptorSet`, Metal argument buffer or slot cache, OpenGL binding table — are transient, cacheable implementation details that may be frame-local. They must not be externally observable through the public API.

---

### 8.1 Responsibilities

A `ResourceSet` is split into two layers with distinct ownership and lifetime.

#### Logical layer (user-visible, long-lived)

- a reference to its parent `PipelineLayout`
- the target `setIndex`
- a CPU-side table of bound resource handles (textures, buffers, samplers)
- a CPU-side constant data blob for ordinary data fields
- a version counter, incremented on every mutation

#### Backend layer (implementation-private, may be frame-local)

- cached backend binding objects (`VkDescriptorSet`, encoded Metal argument buffer, OpenGL binding table)
- per-frame upload allocations for constant data
- dirty/staleness flags tracking whether the backend cache reflects current logical state

---

### 8.2 Separate constants from resource handles

A `ResourceSet` contains two fundamentally different things:

#### Constant / ordinary data
Examples: `float4 baseColor`, `float4x4 model`, `float time`

#### Resource handles
Examples: `Texture2D albedoTex`, `SamplerState albedoSamp`, `StructuredBuffer<Foo> someBuffer`

These should not be written through the same low-level storage path.

---

### 8.3 Constant data container

`ParameterBlockData` is a **raw blob container** for CPU-side constant data. It has no knowledge of field names or reflection — it operates purely on byte offsets and sizes. All field-path resolution is the exclusive responsibility of `ShaderParameterWriter` (ShaderSystem.md §4).

```cpp
class ParameterBlockData
{
public:
    // Write `size` bytes from `data` at byte `offset` into the constant blob.
    // Caller is responsible for correct offset/size (obtained from ReflectedField).
    void setRaw(uint32_t offset, const void* data, size_t size);

    template<typename T>
    void set(uint32_t offset, const T& value) { setRaw(offset, &value, sizeof(T)); }

    const void* data() const;
    size_t      size() const;
    void        resize(size_t bytes);   // called once at ResourceSet creation time
};
```

**Do not add path-based methods to this class.** Any call site that has a field path should go through `ShaderParameterWriter`, which resolves the path to a byte offset and then calls `setRaw()` on the internal `ParameterBlockData`.

---

### 8.4 Resource binding value types

```cpp
struct BufferBinding
{
    Buffer* buffer = nullptr;
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct TextureBinding
{
    Texture* texture = nullptr;
    TextureView* view = nullptr;
};

struct SamplerBinding
{
    Sampler* sampler = nullptr;
};
```

---

### 8.5 ResourceSet public interface

```cpp
class ResourceSet
{
public:
    PipelineLayout* getLayout() const;
    uint32_t getSetIndex() const;

    ParameterBlockData& constants();
    const ParameterBlockData& constants() const;

    // Index-based resource binding.
    // For named / path-based access use ShaderParameterWriter (ShaderSystem.md §4),
    // which is the canonical ergonomic interface.
    void setBuffer(uint32_t binding, const BufferBinding&);
    void setTexture(uint32_t binding, const TextureBinding&);
    void setSampler(uint32_t binding, const SamplerBinding&);

    // Returns a monotonically increasing counter, incremented on every mutation.
    // Backend implementations compare this against their last-processed version to
    // detect staleness. Dirty tracking is entirely internal to the backend layer.
    uint32_t version() const;
};
```

---

### 8.6 Dirty state tracking (backend-internal)

Dirty state is managed entirely within the backend implementation layer. It is **not** exposed on the public `ResourceSet` interface. The only externally visible signal is `version()`, which backend code compares against a cached value.

Backend implementations should track at least:

- **constants dirty**: the CPU blob has changed since the last GPU upload
- **bindings dirty**: a resource handle has changed since the last backend descriptor/argument-buffer update
- **backend cache dirty**: the backend object needs re-recording for the current frame slot

---

## 9. Vertex Input System

This system is separate from `ResourceSet`.

---

### 9.1 Vertex layout

```cpp
struct VertexAttributeDesc
{
    uint32_t location = 0;
    Format format;
    uint32_t offset = 0;
    uint32_t bufferSlot = 0;
};

struct VertexBufferLayoutDesc
{
    uint32_t stride = 0;
    bool perInstance = false;
};

struct VertexInputLayoutDesc
{
    std::vector<VertexAttributeDesc> attributes;
    std::vector<VertexBufferLayoutDesc> buffers;
};
```

---

### 9.2 VertexInputLayout object

```cpp
class VertexInputLayout
{
public:
    const VertexInputLayoutDesc& getDesc() const;
};
```

---

### 9.3 MeshBinding

`MeshBinding` is a draw-call-level snapshot of the geometry buffers for one mesh. `VertexInputLayout` is **not** included here — it is part of `GraphicsPipeline` and is baked at pipeline creation time. The same `MeshBinding` is valid for any pipeline whose vertex layout is compatible.

```cpp
enum class IndexType
{
    UInt16,
    UInt32,
};

struct MeshBinding
{
    std::vector<Buffer*> vertexBuffers;
    Buffer*   indexBuffer = nullptr;
    IndexType indexType   = IndexType::UInt32;
};
```

---

## 10. Pipeline Objects

### 10.1 Pipeline State Types

These types are referenced by `GraphicsPipelineDesc`.

```cpp
enum class PrimitiveTopology
{
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};

// --- Raster state ---

enum class CullMode  { None, Front, Back };
enum class FrontFace { CW, CCW };
enum class FillMode  { Solid, Wireframe };

struct RasterState
{
    CullMode  cullMode             = CullMode::Back;
    FrontFace frontFace            = FrontFace::CCW;
    FillMode  fillMode             = FillMode::Solid;
    bool      depthClampEnable     = false;
    bool      depthBiasEnable      = false;
    float     depthBiasConstant    = 0.0f;
    float     depthBiasSlopeFactor = 0.0f;
};

// --- Depth / stencil state ---

enum class CompareOp
{
    Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always,
};

struct DepthStencilState
{
    bool      depthTestEnable  = false;
    bool      depthWriteEnable = false;
    CompareOp depthCompareOp   = CompareOp::Less;
    // Stencil state is not included in v1.
};

// --- Blend state ---

enum class BlendFactor
{
    Zero,          One,
    SrcColor,      OneMinusSrcColor,
    DstColor,      OneMinusDstColor,
    SrcAlpha,      OneMinusSrcAlpha,
    DstAlpha,      OneMinusDstAlpha,
};

enum class BlendOp { Add, Subtract, ReverseSubtract, Min, Max };

struct BlendState
{
    bool        blendEnable    = false;
    BlendFactor srcColorFactor = BlendFactor::One;
    BlendFactor dstColorFactor = BlendFactor::Zero;
    BlendOp     colorBlendOp   = BlendOp::Add;
    BlendFactor srcAlphaFactor = BlendFactor::One;
    BlendFactor dstAlphaFactor = BlendFactor::Zero;
    BlendOp     alphaBlendOp   = BlendOp::Add;
    uint8_t     colorWriteMask = 0xF;   // RGBA channels
};
```

---

### 10.2 GraphicsPipeline

A `GraphicsPipeline` encapsulates:

- shader stages
- pipeline layout
- vertex input layout
- raster state
- blend state
- depth/stencil state
- attachment formats
- primitive topology

```cpp
struct GraphicsPipelineDesc
{
    PipelineLayout*    pipelineLayout = nullptr;
    ShaderProgram*     shaderProgram  = nullptr;
    VertexInputLayout* vertexInput    = nullptr;

    // v1: a single BlendState applies uniformly to all color attachments.
    // Per-attachment independent blend state is out of scope for v1.
    // colorFormats may list multiple attachments (MRT), but they all share this one BlendState.
    BlendState        blendState;
    DepthStencilState depthStencilState;
    RasterState       rasterState;
    PrimitiveTopology topology;

    std::vector<Format> colorFormats;  // one entry per color attachment; MRT supported at format level
    Format depthFormat = Format::Unknown;
};
```

---

### 10.3 ComputePipeline

```cpp
struct ComputePipelineDesc
{
    PipelineLayout* pipelineLayout = nullptr;
    ShaderProgram*  shaderProgram  = nullptr;
};
```

---

## 11. Command Recording Model

The command interface is explicit and backend-neutral.

---

### 11.1 RenderingInfo

`RenderingInfo` is passed to `CommandList::beginRendering()` and fully describes the render pass: which attachments to write, what to do with their contents on load and store, and the drawable region.

```cpp
enum class LoadOp
{
    Load,     // preserve existing attachment contents
    Clear,    // clear to the specified value before rendering
    DontCare, // prior contents undefined; enables tiler optimizations on Metal / mobile GPUs
};

enum class StoreOp
{
    Store,    // write rendered results to backing memory
    DontCare, // results not needed after the pass; enables tiler optimizations
};

struct ClearColor        { float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f; };
struct ClearDepthStencil { float depth = 1.0f; uint8_t stencil = 0; };

struct ColorAttachmentInfo
{
    TextureView* view       = nullptr;
    LoadOp       loadOp     = LoadOp::Load;
    StoreOp      storeOp    = StoreOp::Store;
    ClearColor   clearValue;
};

struct DepthAttachmentInfo
{
    TextureView*      view      = nullptr;  // nullptr = no depth/stencil attachment
    LoadOp            loadOp    = LoadOp::Load;
    StoreOp           storeOp   = StoreOp::Store;
    ClearDepthStencil clearValue;
};

struct Rect2D
{
    int32_t  x = 0, y = 0;
    uint32_t width = 0, height = 0;
};

struct RenderingInfo
{
    std::vector<ColorAttachmentInfo> colorAttachments;
    DepthAttachmentInfo              depthAttachment;  // .view == nullptr → no depth
    Rect2D                           renderArea;
};
```

`RenderingInfo` maps directly to Vulkan's dynamic rendering (`VkRenderingInfo`), Metal's `MTLRenderPassDescriptor`, and OpenGL's framebuffer object attachment configuration.

---

### 11.2 CommandList interface

```cpp
class CommandList
{
public:
    void beginRendering(const RenderingInfo&);
    void endRendering();

    void bindGraphicsPipeline(GraphicsPipeline*);
    void bindComputePipeline(ComputePipeline*);

    void bindResourceSet(uint32_t setIndex, ResourceSet*);
    void pushConstants(ShaderStage stageMask, uint32_t offset, uint32_t size, const void* data);

    // Convenience: expands MeshBinding into bindVertexBuffers() + bindIndexBuffer().
    // vertexOffsets may be nullptr (all offsets default to 0).
    void bindMesh(const MeshBinding&, const uint64_t* vertexOffsets = nullptr);

    // Low-level vertex/index binding for partial or custom binding scenarios.
    void bindVertexBuffers(
        uint32_t firstSlot,
        Buffer* const* buffers,
        uint32_t count,
        const uint64_t* offsets);

    void bindIndexBuffer(Buffer*, uint64_t offset, IndexType);

    void setViewport(float x, float y, float w, float h, float zmin, float zmax);
    void setScissor(int32_t x, int32_t y, uint32_t w, uint32_t h);

    void draw(uint32_t vertexCount, uint32_t firstVertex);
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset);

    void dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ);

    // -----------------------------------------------------------------------
    // Layer 1 barrier primitives — INTERNAL USE ONLY.
    // Do not call these from renderer code. Use ResourceStateTracker (§11.5)
    // at v1, or rely on Render Graph pass declarations in future versions.
    // These exist so that Layer 2 / Layer 3 can emit barriers without
    // bypassing the CommandList abstraction.
    // -----------------------------------------------------------------------
    void textureBarrier(Texture*,     TextureState oldState, TextureState newState,
                        ShaderStage srcStage, ShaderStage dstStage);
    void bufferBarrier (Buffer*,      BufferState  oldState, BufferState  newState,
                        ShaderStage srcStage, ShaderStage dstStage);
};
```

---

### 11.3 Binding order expectations

Typical graphics draw sequence:

1. begin rendering
2. bind graphics pipeline
3. bind frame resource set
4. bind material resource set
5. bind object resource set
6. bind mesh (or bind vertex/index buffers individually)
7. draw indexed
8. end rendering

---

### 11.4 Resource States

Layer 1 barrier primitives (§11.2) operate on explicit resource state enums. These are the vocabulary for expressing GPU memory and execution dependencies at the RHI level.

```cpp
enum class TextureState
{
    Undefined,          // initial / don't-care; contents are discarded on transition out
    RenderTarget,       // color attachment write
    DepthStencil,       // depth/stencil attachment read/write
    ShaderRead,         // sampled read in any shader stage
    ShaderReadWrite,    // storage image read/write
    CopySource,         // source for copy/blit operations
    CopyDest,           // destination for copy/blit operations
    Present,            // ready for swapchain present
};

enum class BufferState
{
    Undefined,          // initial / don't-care
    VertexIndex,        // vertex buffer or index buffer read
    UniformRead,        // uniform / constant buffer read
    StorageRead,        // storage buffer read-only
    StorageReadWrite,   // storage buffer read/write
    CopySource,         // source for copy operations
    CopyDest,           // destination for copy operations
    IndirectArgument,   // indirect draw/dispatch argument buffer
};
```

---

### 11.5 ResourceStateTracker  *(Layer 2 — v1 synchronisation API)*

`ResourceStateTracker` is the **Layer 2** synchronisation primitive for v1 rendering code. It sits above the raw barrier calls in §11.2 and tracks the last-known state of each resource, batching transitions and flushing them as a single barrier group before execution.

> **Forward note**: in a future version the Render Graph (see `docs/design/RenderGraph.md`, planned) will replace all manual barrier calls by deriving transitions from declared pass read/write sets. `ResourceStateTracker` is an intentionally minimal interim API; its surface is kept small so that replacing it with Render Graph pass declarations is low-friction.

**Semantic rules (implementers must follow these exactly):**

1. `ResourceStateTracker` tracks the **last known logical state within the current recording scope** — it is not a global device-level resource state database. Its knowledge is scoped to what has been explicitly declared since the last `reset()`.
2. `reset()` discards all tracked state. After `reset()`, any resource that has not been transitioned is treated as `Undefined` state (for textures) or `Undefined` (for buffers) until explicitly seeded via `transition()`. The caller must seed known initial states after `reset()` if they differ from `Undefined`.
3. `transition(resource, A)` followed by `transition(resource, B)` before the next `flushBarriers()` is folded into a single transition from the last-known state to `B`. Intermediate states `A` are not observed by the GPU.
4. `transition(resource, S)` where `S` equals the resource's currently tracked state is a no-op — no barrier is emitted for that resource.
5. Swapchain images returned by `acquireNextImage()` have **no assumed prior state** — the caller must explicitly `transition()` them to the desired state before use.

```cpp
class ResourceStateTracker
{
public:
    // Declare the desired state for a resource.
    // Does not emit a barrier immediately — barriers are batched until flushBarriers().
    void transition(Texture*, TextureState newState);
    void transition(Buffer*,  BufferState  newState);

    // Emit all pending barriers into `cmd` as a single batch.
    // Call this immediately before beginRendering() or Device::submit().
    void flushBarriers(CommandList* cmd);

    // Discard all tracked state. Resources will be treated as Undefined until
    // re-seeded via transition(). Call at the start of each frame or after submit.
    void reset();
};
```

---

## 12. Object Lifetime and Ownership

All public RHI factory methods must return owned objects with unambiguous semantics. Raw owning pointers are not permitted.

---

### 12.1 Scope\<T\>

The engine defines a single ownership alias used throughout the RHI:

```cpp
template<typename T>
using Scope = std::unique_ptr<T>;
```

All `Device` factory methods return `Scope<T>`. The caller is the sole owner. Destruction happens automatically when the `Scope<T>` goes out of scope, which invokes the backend-private virtual destructor chain — there is no separate explicit `destroy` call.

---

### 12.2 Ownership rules

| Object | Ownership | Acquired / released via |
|--------|-----------|-------------------------|
| `Buffer`, `Texture`, `TextureView`, `Sampler` | `Scope<T>` | `Device::create*()` / destructor |
| `ShaderProgram`, `PipelineLayout`, `ResourceSet` | `Scope<T>` | `Device::create*()` / destructor |
| `VertexInputLayout`, `GraphicsPipeline`, `ComputePipeline` | `Scope<T>` | `Device::create*()` / destructor |
| `Swapchain` | `Scope<T>` | `Device::createSwapchain()` / destructor |
| Swapchain images / views | non-owning `T*` | `Swapchain::getImage()` / `getImageView()`; lifetime tied to `Swapchain` |
| `CommandList` | non-owning `T*` | `Device::beginCommandList()` → `Device::submit()` |
| `FrameContext` | non-owning `T*` | `Device::beginFrame()` → `Device::endFrame()` |
| Shared resources | `std::shared_ptr<T>` | engine-side only, where true shared ownership is needed |

- Internal backend handles (`VkImage`, `VkDeviceMemory`, `MTLTexture`, etc.) remain RAII-managed inside their owning RHI objects and are never exposed publicly.
- Non-owning `T*` parameters in function calls must not be deleted by the callee.

---

## 13. Frame Resource Upload Model

Multi-backend rendering requires a clear model for how CPU-side data in `ResourceSet` objects is uploaded to the GPU each frame. Without this model, backends face ambiguity about when to allocate, upload, and release transient GPU memory.

---

### 13.1 The upload problem

A `ResourceSet` holds a CPU-side constants blob and resource handles. At draw time, backends need GPU-accessible memory for the constants. This memory:

- may be written every frame (frame/object sets) or rarely (static material sets)
- must not be overwritten while the GPU is still reading from it
- should be sub-allocated from a ring buffer, not individually heap-allocated per set per frame

---

### 13.2 FrameContext

A `FrameContext` represents one in-flight frame slot. The engine maintains a fixed number of instances (typically 2–3 for double/triple buffering).

```cpp
// Forward declaration — full definition is backend-private.
class FrameContext;
```

`Device` manages the pool of `FrameContext` objects. The renderer acquires one at the start of each frame and releases it once the GPU signals completion for that frame. The `beginFrame()` / `endFrame()` methods are part of the full `Device` interface defined in §14.

**Ownership boundary — Swapchain vs. FrameContext:**

These two objects have deliberately separate responsibilities:

| Concern | Owner |
|---------|-------|
| Presentable image acquire / present / resize | `Swapchain` |
| CPU transient upload allocation (ring buffer slot) | `FrameContext` |
| GPU frame fence / in-flight slot management | `FrameContext` |
| Backbuffer image lifetime | `Swapchain` (images are swapchain-owned) |

`acquireNextImage()` is called before `beginFrame()` in the frame loop (see §15) because acquiring a presentable image is a swapchain-level concern that may block on the presentation engine, independent of which upload slot the CPU is working on. The two operations are not required to be atomic with each other.

---

### 13.3 Transient upload allocator

Each `FrameContext` owns a **transient upload allocator** — a GPU-visible ring buffer or slab from which constant data is sub-allocated for the duration of the frame.

Responsibilities:
- sub-allocate aligned slices for `ResourceSet` constant uploads
- guarantee that allocations are not reused until the GPU fence for that frame has signaled
- return a `(buffer, byteOffset)` pair suitable for use as a uniform buffer binding

This allocator is entirely backend-private and is not visible through the public API.

---

### 13.4 Upload lifetime rules

| Data type | Upload timing | Lifetime |
|-----------|--------------|----------|
| Frame / scene constants | Once per frame, before first draw | Entire frame |
| Object constants | Per draw or per batch | Until next draw using the same set |
| Material constants | On version change only | Until next material mutation |
| Resource handles (textures, samplers) | On first bind or on version change | Persistent until resource destroyed |

The backend compares `ResourceSet::version()` against the version it last processed to determine whether re-upload is needed for the current frame slot.

---

### 13.5 Multi-frame in-flight constraints

Because multiple frames may be in flight simultaneously:

- each `FrameContext` must use a separate upload allocation region
- a `ResourceSet` that changes every frame (e.g. object set) must not share upload memory across frame slots
- a `ResourceSet` that rarely changes (e.g. static material) may cache its last-uploaded slice and reuse it, provided its `version()` has not changed and the `FrameContext` that produced the upload is still valid

The backend is responsible for per-`FrameContext` upload tracking. The public `ResourceSet` API does not change based on the number of in-flight frames.

---

## 14. Suggested Public Interface Sketch

Below is a compact overview of the recommended public API shape.

All factory methods return `Scope<T>` (see §12.1). `CommandList` and `FrameContext` use non-owning raw pointers with bounded lifetimes.

```cpp
class Device
{
public:
    virtual Scope<Swapchain>          createSwapchain         (const SwapchainDesc&, const NativeWindowHandle&) = 0;

    virtual Scope<Buffer>             createBuffer            (const BufferDesc&) = 0;
    virtual Scope<Texture>            createTexture           (const TextureDesc&) = 0;
    virtual Scope<TextureView>        createTextureView       (Texture*, const TextureViewDesc&) = 0;
    virtual Scope<Sampler>            createSampler           (const SamplerDesc&) = 0;

    virtual Scope<ShaderProgram>      createShaderProgram     (const CompiledShaderProgramDesc&) = 0;
    virtual Scope<PipelineLayout>     createPipelineLayout    (const PipelineLayoutDesc&) = 0;
    virtual Scope<ResourceSet>        createResourceSet       (PipelineLayout*, uint32_t setIndex) = 0;

    virtual Scope<VertexInputLayout>  createVertexInputLayout (const VertexInputLayoutDesc&) = 0;

    virtual Scope<GraphicsPipeline>   createGraphicsPipeline  (const GraphicsPipelineDesc&) = 0;
    virtual Scope<ComputePipeline>    createComputePipeline   (const ComputePipelineDesc&) = 0;

    // CommandList: non-owning; lifetime is beginCommandList() → submit().
    virtual CommandList*  beginCommandList() = 0;
    virtual void          submit(CommandList*) = 0;

    // FrameContext: non-owning; lifetime is beginFrame() → endFrame().
    virtual FrameContext* beginFrame() = 0;
    virtual void          endFrame(FrameContext*) = 0;
};
```

---

## 15. Frame Usage Example

A typical frame with a swapchain, `ResourceStateTracker`, and multi-set binding:

```cpp
// ── Initialisation (once) ──────────────────────────────────────────────────
Scope<Swapchain> swapchain = device->createSwapchain(
    SwapchainDesc{ windowWidth, windowHeight, Format::BGRA8_UNORM, 2, true },
    nativeWindowHandle);

ResourceStateTracker tracker;

// ── Per-frame loop ─────────────────────────────────────────────────────────
while (!quit)
{
    // 1. Acquire swapchain image
    uint32_t imageIndex = swapchain->acquireNextImage();
    Texture*     backbuffer     = swapchain->getImage    (imageIndex);
    TextureView* backbufferView = swapchain->getImageView(imageIndex);

    // 2. Begin frame (acquires a FrameContext slot; blocks if all slots are in flight)
    FrameContext* frame = device->beginFrame();

    // 3. Update CPU-side resource sets (via ShaderParameterWriter — see ShaderSystem.md §4)
    writer.setMatrix4x4(*frameSet,    "gFrame.viewProj",     viewProj);
    writer.setFloat4   (*frameSet,    "gFrame.cameraPos",    cameraPos);
    writer.setFloat    (*frameSet,    "gFrame.time",         time);

    writer.setFloat4   (*materialSet, "gMaterial.baseColor", baseColor);
    writer.setTexture  (*materialSet, albedoBinding,         albedoTexture);
    writer.setSampler  (*materialSet, samplerBinding,        linearSampler);

    writer.setMatrix4x4(*objectSet,   "gObject.model",       modelMatrix);

    // 4. Record commands
    CommandList* cmd = device->beginCommandList();

    // Transition resources to their required states before rendering
    tracker.transition(backbuffer,  TextureState::RenderTarget);
    tracker.transition(shadowMap,   TextureState::ShaderRead);
    tracker.flushBarriers(cmd);

    ColorAttachmentInfo colorAtt;
    colorAtt.view       = backbufferView;
    colorAtt.loadOp     = LoadOp::Clear;
    colorAtt.storeOp    = StoreOp::Store;
    colorAtt.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    RenderingInfo renderingInfo;
    renderingInfo.colorAttachments = { colorAtt };
    renderingInfo.renderArea       = { 0, 0, swapchain->width(), swapchain->height() };

    cmd->beginRendering(renderingInfo);
    cmd->bindGraphicsPipeline(pipeline);
    cmd->bindResourceSet(0, frameSet);
    cmd->bindResourceSet(1, materialSet);
    cmd->bindResourceSet(2, objectSet);
    cmd->bindMesh(meshBinding);
    cmd->drawIndexed(indexCount, 0, 0);
    cmd->endRendering();

    // Transition backbuffer to present state before submission
    tracker.transition(backbuffer, TextureState::Present);
    tracker.flushBarriers(cmd);

    // 5. Submit and present
    device->submit(cmd);
    swapchain->present(imageIndex);

    // 6. End frame (releases the FrameContext slot back to the pool)
    device->endFrame(frame);

    tracker.reset();
}
```

This renderer code is backend-agnostic — no backend-specific types or calls appear above the `Device`/`Swapchain`/`CommandList` boundary.

---

## 16. Recommended Implementation Order

To reduce complexity and validate the architecture early, implementation should proceed in a vertical slice.

### Phase 1: Core architecture
Implement:

- Device
- Buffer
- Texture
- Sampler
- ShaderProgram
- PipelineLayout
- ResourceSet
- VertexInputLayout
- GraphicsPipeline
- CommandList

### Phase 2: Shader pipeline
Implement:

- Slang compilation
- reflection extraction
- neutral reflection conversion
- parameter writer
- backend code generation

### Phase 3: Vulkan first
Implement the full Vulkan path first because it is the cleanest mapping to the public model.

### Phase 4: Metal
Implement Metal direct-slot mode first.

### Phase 5: OpenGL
Implement OpenGL compatibility layer for the first supported shader subset.

### Phase 6: Renderer demo
Create a minimal renderer test:
- textured mesh
- camera
- one material
- multiple objects

### Phase 7: Optional improvements
After first end-to-end success:
- Metal argument buffers for material sets
- shader hot reload
- compute pipeline path
- material system expansion
- render graph integration

---

## 17. What This Architecture Explicitly Avoids

This document intentionally avoids the following in version 1:

- bindless-first public abstraction
- universal descriptor heap abstraction
- fully generic residency/heap aliasing abstraction
- vertex input folded into generic resource sets
- hand-written duplicated binding tables
- backend-specific shader layout hardcoding in renderer code

These are excluded to keep the architecture coherent and implementable.

---

## 18. Risks and Tradeoffs

### 18.1 OpenGL feature ceiling
OpenGL support will always be the most fragile path in this architecture.

This is acceptable because OpenGL is defined as a compatibility backend.

### 18.2 Reflection-driven complexity
Relying on reflection introduces some system complexity, but it is still much better than maintaining duplicate truth sources manually.

### 18.3 Backend divergence over time
Vulkan and Metal may later deserve more backend-specific optimization paths. This architecture supports that by keeping the public model neutral while allowing backend-private caching and translation.

### 18.4 Constant data representation differences
Different backends may store parameter-block constant data differently internally, but the public interface should keep that invisible behind `ParameterBlockData` and `ResourceSet`.

---

## 19. Final Architectural Summary

The final architecture can be summarized in one sentence:

> Build the engine's public rendering interface around explicit pipeline/resource-set concepts, derive all shader-visible layout from Slang reflection, use Vulkan as the conceptual reference backend, map Metal naturally through slots and optional argument buffers, and treat OpenGL as a compatibility translation layer.

In practical terms, that means:

- **Slang defines shader parameter structure** (see ShaderSystem.md)
- **reflection defines engine-visible layout** (see ShaderSystem.md §2)
- **PipelineLayout defines the resource interface** (§7)
- **ResourceSet holds one logical parameter group instance** (§8)
- **CommandList binds sets explicitly** (§11)
- **vertex/index input remains separate** (§9)
- **backend implementations translate, but do not redefine, the public abstraction** (see RHI_Backend_*.md)

---

## 20. Recommended Next Steps

The next implementation artifacts after this document should be:

1. a **C++ header-level RHI API draft**
2. a **Slang reflection conversion module design** (see ShaderSystem.md §5)
3. a **minimal shader example set**:
   - `FrameParams`
   - `MaterialParams`
   - `ObjectParams`
   - one unlit or PBR-lite shader

These three together will turn this design from architecture into a buildable skeleton.
