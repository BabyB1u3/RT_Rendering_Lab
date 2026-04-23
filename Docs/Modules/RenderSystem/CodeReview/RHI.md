# RHI Code Review

> Historical note: this review predates the retirement of the OpenGL backend from the project mainline. Any OpenGL-specific findings below are archival and no longer describe active shipping code.

Code review of the `Src/Render/RHI/` surface after M3 Batch 1 landed on
2026-04-20. Batch 1 extracted `Buffer` / `Texture` / `TextureView` / `Sampler`
out of the shared `RHIInternal::Shell*` fallback path into real Vulkan / OpenGL
/ Metal implementations. M3 Batch 2 (VertexInputLayout + GraphicsPipeline) and
Batch 3 (draw path + triangle demo) are not covered here.

> **Overall assessment at the time**: all three backends then under development owned their resources with
> matching public-API semantics, mapping helpers are clean and largely
> symmetric, and the `TRANSITIONAL(M3)` markers on the Vulkan raw-memory path
> are in the right spot. Three real bugs (C1–C3) sit in the "desc field
> silently ignored" family and must be fixed before Batch 2. H1 (Vulkan
> resource creation coupled to swapchain initialization) is the biggest
> structural issue and will block the triangle-demo flow. Everything else is
> polish, documentation, or forward-compat hygiene.

---

## Table of Contents

- [RHI Code Review](#rhi-code-review)
  - [Table of Contents](#table-of-contents)
  - [Summary](#summary)
  - [High Severity](#high-severity)
    - [H2. No Debug-Name Application On Any Backend](#h2-no-debug-name-application-on-any-backend)
  - [Medium Severity](#medium-severity)
    - [M1. OpenGL CreateBuffer Uses Mutable Storage And Drops UsageMask](#m1-opengl-createbuffer-uses-mutable-storage-and-drops-usagemask)
    - [M2. Metal Sampler Forces supportArgumentBuffers = YES Globally](#m2-metal-sampler-forces-supportargumentbuffers--yes-globally)
    - [M3. OpenGLTextureView::m\_OwnsView Is Dead Flexibility](#m3-opengltextureviewm_ownsview-is-dead-flexibility)
    - [M4. Sampler Feature-Parity Stubs Lack TRANSITIONAL Markers](#m4-sampler-feature-parity-stubs-lack-transitional-markers)
    - [M5. ShellDeviceBase::Create\* Is Dead Code For Real Backends](#m5-shelldevicebasecreate-is-dead-code-for-real-backends)
    - [M6. Format Helpers Return A Poison Value In Release Builds](#m6-format-helpers-return-a-poison-value-in-release-builds)
  - [Low Severity](#low-severity)
    - [L1. Size And Extent Clamps Diverge Across Backends](#l1-size-and-extent-clamps-diverge-across-backends)
    - [L2. Metal Resource-Options Bit-Packing Is Cryptic](#l2-metal-resource-options-bit-packing-is-cryptic)
    - [L3. Metal Retain/Release Dance Needs A One-Line Comment](#l3-metal-retainrelease-dance-needs-a-one-line-comment)
    - [L4. MTLTextureUsageUnknown Starting Value Is Confusing](#l4-mtltextureusageunknown-starting-value-is-confusing)
    - [L5. Device/Resource Ownership Invariant Is Not Encoded In Code](#l5-deviceresource-ownership-invariant-is-not-encoded-in-code)
  - [Strengths](#strengths)

---

## Summary

| Category | Count |
| -------- | ----- |
| Critical | 3     |
| High     | 4     |
| Medium   | 6     |
| Low      | 5     |

| #   | Finding                                                 | Recommended fix timing      |
| --- | ------------------------------------------------------- | --------------------------- |
| H2  | No debug-name application                               | Before M3 exit              |
| M1  | OpenGL buffer uses mutable storage; `UsageMask` ignored | Comment now, refactor at M4 |
| M3  | `OpenGLTextureView::m_OwnsView` always true             | Cleanup pass                |
| M4  | Sampler stubs missing `TRANSITIONAL` markers            | With H3                     |
| M5  | `ShellDeviceBase::Create*` dead for real backends       | Decide intent               |
| M6  | Format helpers silently return poison in release        | Cleanup pass                |
| L1  | Size/extent clamps diverge                              | With H1 cleanup             |
| L2  | Metal options bit-packing cryptic                       | Cleanup pass                |
| L3  | Metal retain/release dance unexplained                  | Cleanup pass                |
| L4  | `MTLTextureUsageUnknown` used as OR seed                | Cleanup pass                |
| L5  | Device ownership invariant not documented in code       | Cleanup pass                |

---
## High Severity
### H2. No Debug-Name Application On Any Backend

**Location:** Vulkan / OpenGL / Metal `Create*` paths

`desc.m_DebugName` is preserved in the stored `*Desc` but never applied to
the native handle. Cheap to add (3 lines per backend) and makes RenderDoc /
Xcode frame capture usable for M3+. Without it, captures show generic
`Buffer_0x7ff…` labels.

**Suggested fix:** a small helper per backend, invoked after handle creation
and before `Scope` construction.

```cpp
// Vulkan (requires vkSetDebugUtilsObjectNameEXT loaded via volk + VK_EXT_debug_utils)
void VulkanDevice::SetDebugName(VkObjectType type, uint64_t handle, std::string_view name)
{
    if (name.empty() || vkSetDebugUtilsObjectNameEXT == nullptr)
        return;
    const std::string zeroTerminated(name);
    VkDebugUtilsObjectNameInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = zeroTerminated.c_str();
    vkSetDebugUtilsObjectNameEXT(m_Device, &info);
}
```

OpenGL: `glObjectLabel(GL_BUFFER, buffer, -1, desc.m_DebugName.c_str());` when
`!desc.m_DebugName.empty()`.

Metal: `[buffer setLabel:[NSString stringWithUTF8String:desc.m_DebugName.c_str()]];`
when non-empty.

---

## Medium Severity

### M1. OpenGL CreateBuffer Uses Mutable Storage And Drops UsageMask

**Location:** [`OpenGLDevice.cpp:287-293`](../../../Src/Render/RHI/Backends/OpenGL/OpenGLDevice.cpp)

Two separate issues sharing a code site:

1. `glNamedBufferData` creates mutable storage with a usage hint. This
   doesn't match Vulkan's "allocate-once, immutable-size" semantics that
   staging-buffer uploads (M4) will expect.
2. `desc.m_UsageMask` (Vertex / Index / Uniform / Storage / CopySrc / CopyDst
   / Indirect) is ignored entirely. With DSA this is technically fine — bind
   targets are set later — but it's opaque without a comment.

**Suggested fix:** add both a clarifying NOTE and a forward-looking
TRANSITIONAL marker.

```cpp
Scope<Buffer> OpenGLDevice::CreateBuffer(const BufferDesc& desc)
{
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);

    // NOTE: with DSA, buffer creation does not require a bind target, so
    // desc.m_UsageMask is intentionally unused here; usage intent is decided
    // at subsequent bind points (glBindBuffer / glBindBufferRange / etc.).

    // TRANSITIONAL(M4): currently uses mutable storage (glNamedBufferData +
    // usage hint) to keep M3 buffer lifetimes simple. When Upload / persistently
    // mapped ring buffers land in M4, switch to:
    //     glNamedBufferStorage(buffer, size, nullptr,
    //         GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT |
    //         GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    // to match Vulkan VMA's "allocate-once, immutable-size" semantics.
    glNamedBufferData(buffer,
                      static_cast<GLsizeiptr>(std::max<uint64_t>(desc.m_Size, 1)),
                      nullptr,
                      ToGLBufferUsage(desc.m_MemoryUsage));
    return CreateScope<OpenGLBuffer>(buffer, desc);
}
```

---

### M2. Metal Sampler Forces supportArgumentBuffers = YES Globally

**Location:** [`MetalDevice.mm:646`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm)

Argument buffers are not used in M3 (or the planned M4). Forcing the flag
can prevent the driver from fast-pathing direct-bind samplers and adds
residency cost.

**Suggested fix:** drop the line; the default is `NO`. Re-enable
conditionally when argument-buffer binding actually lands in M5+.

```objc
// samplerDesc.supportArgumentBuffers = YES;   // <-- delete
```

---

### M3. OpenGLTextureView::m_OwnsView Is Dead Flexibility

**Location:** [`OpenGLDevice.cpp:166-186, 361`](../../../Src/Render/RHI/Backends/OpenGL/OpenGLDevice.cpp)

The flag is always constructed as `true`; no non-owning construction path
exists. YAGNI — add back when an actual second construction site appears.

**Suggested fix:** remove the field, the constructor parameter, and the
dtor guard.

```cpp
class OpenGLTextureView final : public TextureView
{
public:
    OpenGLTextureView(Texture* texture, GLuint textureView, GLenum target, const TextureViewDesc& desc)
        : m_Texture(texture), m_TextureView(textureView), m_Target(target), m_Desc(desc) {}

    ~OpenGLTextureView() override
    {
        if (m_TextureView != 0)
            glDeleteTextures(1, &m_TextureView);
    }
    // ... getters unchanged; m_OwnsView field removed
};
```

`CreateTextureView` drops the `true` argument.

---

### M4. Sampler Feature-Parity Stubs Lack TRANSITIONAL Markers

**Location:**
- Vulkan: [`VulkanDevice.cpp:1247, 1250`](../../../Src/Render/RHI/Backends/Vulkan/VulkanDevice.cpp) — `compareEnable = VK_FALSE`, `borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK`
- OpenGL: [`OpenGLDevice.cpp:364`](../../../Src/Render/RHI/Backends/OpenGL/OpenGLDevice.cpp) — `GL_TEXTURE_BORDER_COLOR` never set (GL default `(0, 0, 0, 0)`)
- Metal: [`MetalDevice.mm:633`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm) — no `compareFunction`, no `borderColor`

Acceptable as M3 stubs; the gap is just invisible from the code alone.

**Suggested fix:** add a `TRANSITIONAL(M?)` marker at each site naming the
`SamplerDesc` extension field that will drive it (e.g., `m_CompareOp` for
shadow-map PCF sampling, `m_BorderColor` for `AddressMode::ClampToBorder`).

---

### M5. ShellDeviceBase::Create\* Is Dead Code For Real Backends

**Location:** [`RHIShellCommon.cpp:303-321`](../../../Src/Render/RHI/Backends/Common/RHIShellCommon.cpp)

Every concrete device overrides all four. Either these stubs serve future
incremental bring-up for new backends (e.g., D3D12, WebGPU), or they're
dead weight.

**Suggested fix:** keep them — they're useful for incremental bring-up where
a new backend lands one resource type at a time — but add a one-block comment
above `ShellDeviceBase` declaring intent. Alternatively, if the backend set
is closed at three, make them pure virtual.

```cpp
// ShellDeviceBase provides stub Create* implementations so early-stage backends
// can compile and run basic bring-up while only some resource types are fully
// implemented. Production backends must override every method; remaining
// Shell* returns indicate the type is not yet landed on that backend.
// As of M3 Batch 1, Buffer / Texture / TextureView / Sampler should no longer
// fall through to the Shell fallback on any production backend — the stubs
// exist for incremental onboarding of future backends only.
```

---

### M6. Format Helpers Return A Poison Value In Release Builds

**Location:** [`VulkanDevice.cpp:208-210`](../../../Src/Render/RHI/Backends/Vulkan/VulkanDevice.cpp),
[`OpenGLDevice.cpp:82-84`](../../../Src/Render/RHI/Backends/OpenGL/OpenGLDevice.cpp),
[`MetalDevice.mm:65-67`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm)

`RTRLAB_ASSERTF(false, ...); return VK_FORMAT_UNDEFINED;` fires the assert in
debug but in release silently returns a poison value. Downstream
`vkCreateImage` / native ctor / GL call fails opaquely far from the real
error.

**Suggested fix:** prefer a macro that does not fall through. If the project
has `RTRLAB_UNREACHABLE`, use it; otherwise abort explicitly.

```cpp
VkFormat ToVkFormat(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM: return VK_FORMAT_R8_UNORM;
        // ... remaining cases
    }
    RTRLAB_UNREACHABLEF("Unsupported Vulkan RHI format {}", static_cast<uint32_t>(format));
}
```

Apply to `ToVkFormat`, `ToMetalPixelFormat`, `ToGLInternalFormat`, and the
other enum-mapping helpers that fall through to a default return.

---

## Low Severity

### L1. Size And Extent Clamps Diverge Across Backends

**Locations:**
- Metal buffer: `std::max(size, 1)` ([`MetalDevice.mm:549`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm))
- OpenGL buffer: `std::max<uint64_t>(size, 1)` ([`OpenGLDevice.cpp:291`](../../../Src/Render/RHI/Backends/OpenGL/OpenGLDevice.cpp))
- Vulkan buffer: no clamp
- Vulkan texture: clamps width/height/depth to `max(1)`
- Metal texture: clamps width/height only; uses `m_Extent.m_Depth` unclamped

**Suggested fix:** centralize in `RHIInternal::SanitizeBufferDesc` /
`SanitizeTextureDesc`, analogous to the existing `SanitizeSwapchainDesc`.

```cpp
namespace RHIInternal
{
BufferDesc SanitizeBufferDesc(const BufferDesc& desc)
{
    RTRLAB_ASSERT_MSG(desc.m_Size > 0, "Buffer size must be positive.");
    return desc;
}

TextureDesc SanitizeTextureDesc(const TextureDesc& desc)
{
    RTRLAB_ASSERT_MSG(desc.m_Extent.m_Width > 0 && desc.m_Extent.m_Height > 0,
                      "Texture width/height must be positive.");
    TextureDesc sanitized = desc;
    if (desc.m_Type != TextureType::Tex3D)
        sanitized.m_Extent.m_Depth = 1;
    sanitized.m_MipLevels = std::max(desc.m_MipLevels, 1u);
    sanitized.m_ArrayLayers = std::max(desc.m_ArrayLayers, 1u);
    return sanitized;
}
} // namespace RHIInternal
```

Each backend's `Create*` prelude becomes `const auto d = SanitizeXxxDesc(desc);`,
and the scattered inline `std::max(..., 1u)` calls go away.

---

### L2. Metal Resource-Options Bit-Packing Is Cryptic

**Location:** [`MetalDevice.mm:551`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm)

`static_cast<MTLResourceOptions>(ToMetalStorageMode(desc.m_MemoryUsage) << MTLResourceStorageModeShift)`
is correct but nontrivial to read.

**Suggested fix:** extract a helper.

```objc
MTLResourceOptions ToMetalBufferResourceOptions(MemoryUsage memoryUsage)
{
    const MTLStorageMode storageMode = ToMetalStorageMode(memoryUsage);
    return static_cast<MTLResourceOptions>(MTLResourceCPUCacheModeDefaultCache) |
           static_cast<MTLResourceOptions>(storageMode << MTLResourceStorageModeShift);
}
```

`CreateBuffer` becomes one line: `MTLResourceOptions options = ToMetalBufferResourceOptions(desc.m_MemoryUsage);`.

---

### L3. Metal Retain/Release Dance Needs A One-Line Comment

**Location:** [`MetalDevice.mm:555-557, 585-587, 628-630, 652-654`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm)

`auto result = CreateScope<MetalBuffer>(buffer, desc); [buffer release]; return result;`
is correct — it balances the `+1` from `newBufferWithLength:` against the
`+1` that the `MetalBuffer` ctor takes internally. But it reads as a
"why is there an explicit release here?" puzzle without context.

**Suggested fix:** one comment on the first occurrence.

```objc
id<MTLBuffer> buffer = [m_Data->m_Device newBufferWithLength:size options:options];
RTRLAB_ASSERT_MSG(buffer != nil, "Failed to create the Metal buffer.");
// newBufferWithLength: returns +1 retain; MetalBuffer's ctor retains again (+2).
// The factory releases the new* retain before returning so Scope holds +1,
// which drops to zero on Scope destruction.
auto result = CreateScope<MetalBuffer>(buffer, desc);
[buffer release];
return result;
```

---

### L4. MTLTextureUsageUnknown Starting Value Is Confusing

**Location:** [`MetalDevice.mm:573`](../../../Src/Render/RHI/Backends/Metal/MetalDevice.mm)

`textureDesc.usage = MTLTextureUsageUnknown; textureDesc.usage |= MTLTextureUsageShaderRead;`
reads as if `Unknown` were a flag (it's actually `0`). Correct behavior,
confusing code.

**Suggested fix:**

```objc
MTLTextureUsage usage = 0;
if ((desc.m_UsageMask & TextureUsage::Sampled) != TextureUsage::None)
    usage |= MTLTextureUsageShaderRead;
if ((desc.m_UsageMask & TextureUsage::Storage) != TextureUsage::None)
    usage |= MTLTextureUsageShaderWrite;
if ((desc.m_UsageMask & TextureUsage::RenderTarget) != TextureUsage::None ||
    (desc.m_UsageMask & TextureUsage::DepthStencil) != TextureUsage::None)
    usage |= MTLTextureUsageRenderTarget;

// Fall back to Unknown when the desc specifies no usage bits, letting the
// driver infer usage from the commands that reference this texture.
textureDesc.usage = usage != 0 ? usage : MTLTextureUsageUnknown;
```

---

### L5. Device/Resource Ownership Invariant Is Not Encoded In Code

**Location:** public RHI surface

RHI.md §12 says `Scope<Buffer/Texture/...>` must be destroyed before the
`Device`, but the invariant lives only in the design doc. Each backend dtor
null-guards its device handle as a safety net (not a contract), and there's
no NOTE in code describing the rule.

**Suggested fix:** add a contract comment at the top of `Device` in
`RHIDevice.h` (or at the top of `ShellDeviceBase`).

```cpp
// Ownership contract (see Docs/Modules/RenderSystem/Design/RHI.md §12):
//  1. Device must outlive all RHI resources it produces (Buffer / Texture /
//     TextureView / Sampler / Pipeline / ResourceSet / Swapchain / CommandList).
//  2. All Scope<T> resources must be released before Device destruction.
//     The backend dtors null-guard native handles as a safety net; relying
//     on that net is UB.
//  3. A CommandList must not reference resources that have been destroyed
//     between Submit and fence completion (GPU-timeline constraint).
```

---

## Strengths

- All three backends now own their resources with matching public-API
  semantics — `Buffer` / `Texture` / `TextureView` / `Sampler` lifetime is
  no longer entangled with the Shell fallback.
- Mapping helpers (`ToVkFormat` / `ToGLInternalFormat` / `ToMetalPixelFormat`,
  `ToVkBufferUsage`, `ToVkImageUsage`, the filter/address-mode trio) are
  clean, symmetric, and localized to the anonymous namespace — easy to
  extend as formats are added.
- The `TRANSITIONAL(M3)` markers on the Vulkan raw-memory path land in the
  right spot and correctly identify the future VMA replacement surface
  (even if they need to get more specific — see H3).
- Vulkan's two-tier memory-type search (`FindMemoryType` with required +
  fallback) is the right shape for covering real-device variance without
  hard-failing on uncommon property combinations.
- OpenGL uses DSA throughout (`glCreateBuffers` / `glCreateTextures` /
  `glCreateSamplers`), matching modern GL 4.5+ expectations; the one
  deliberate exception (`glGenTextures` for `glTextureView`) is driven by
  GL semantics, not oversight.
- Metal's retain/release accounting through the backend classes is
  consistent across all four types — the pattern just needs a one-line
  explanatory comment (L3).
