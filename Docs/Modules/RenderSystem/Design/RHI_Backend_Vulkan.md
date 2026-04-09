# RHI Backend — Vulkan

This document covers the Vulkan-specific implementation strategy for the RHI layer.

Vulkan is the **architectural reference backend** — the public RHI model was designed around Vulkan concepts. Mappings from the neutral public API to Vulkan are therefore the most direct of the three backends.

For the public RHI API being mapped, see [RHI.md](RHI.md). For shader compilation outputs consumed by this backend, see [ShaderSystem.md](ShaderSystem.md).

---

## 1. Memory Allocation — VMA

The Vulkan backend uses **[Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)** for all `VkBuffer` and `VkImage` allocations. VMA is an AMD-maintained, single-header C library that handles sub-allocation, memory type selection, and heap budget tracking on top of raw Vulkan memory APIs.

VMA is entirely backend-private. No public RHI types or headers change; the public `MemoryUsage` enum (RHI.md §6.2) maps directly to VMA usage flags.

---

### 1.1 Why VMA

Vulkan requires the application to:

- enumerate and select `VkMemoryType` manually from `VkPhysicalDeviceMemoryProperties`
- call `vkAllocateMemory` per resource, subject to a hard driver limit (typically 4 096 allocations total)
- implement sub-allocation to share `VkDeviceMemory` blocks across multiple resources
- respect alignment requirements individually per resource type

Implementing a correct sub-allocator is a significant standalone engineering effort and is not the core value of this engine. VMA solves all of the above problems with a well-tested, widely deployed implementation (used by Godot, Filament, and many commercial engines).

---

### 1.2 MemoryUsage mapping

| RHI `MemoryUsage`   | VMA flag                              | Use case |
|---------------------|---------------------------------------|----------|
| `GpuOnly`           | `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE` | Textures, GPU-side vertex/index/storage buffers |
| `CpuToGpu`          | `VMA_MEMORY_USAGE_AUTO` + `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` | Staging buffers, transient upload ring buffers |
| `GpuToCpu`          | `VMA_MEMORY_USAGE_AUTO` + `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT` | Readback buffers |

Use `VMA_MEMORY_USAGE_AUTO` (VMA ≥ 3.0) rather than the deprecated `VMA_MEMORY_USAGE_GPU_ONLY` / `VMA_MEMORY_USAGE_CPU_TO_GPU` constants.

---

### 1.3 Initialisation

`VmaAllocator` is created once during `Device` initialisation and destroyed during `Device` shutdown. It is stored as a private member of the Vulkan `Device` implementation.

```cpp
VmaAllocatorCreateInfo allocatorInfo{};
allocatorInfo.physicalDevice   = physicalDevice;
allocatorInfo.device           = device;
allocatorInfo.instance         = instance;
allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

// Enable VK_EXT_memory_budget if available — lets VMA respect OS-reported heap budgets.
if (memoryBudgetExtensionAvailable)
    allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

vmaCreateAllocator(&allocatorInfo, &g_allocator);
```

---

### 1.4 Buffer allocation

```cpp
VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
bufInfo.size  = desc.size;
bufInfo.usage = toVkBufferUsage(desc.usageMask);   // engine flags → VkBufferUsageFlags

VmaAllocationCreateInfo allocInfo{};
allocInfo.usage = toVmaUsage(desc.memoryUsage);    // see §1.2 mapping table
allocInfo.flags = toVmaAllocFlags(desc.memoryUsage);

VkBuffer      vkBuffer;
VmaAllocation allocation;
vmaCreateBuffer(g_allocator, &bufInfo, &allocInfo, &vkBuffer, &allocation, nullptr);
```

---

### 1.5 Image allocation

```cpp
VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
// ... fill from TextureDesc ...

VmaAllocationCreateInfo allocInfo{};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;  // textures are always GpuOnly

VkImage       vkImage;
VmaAllocation allocation;
vmaCreateImage(g_allocator, &imgInfo, &allocInfo, &vkImage, &allocation, nullptr);
```

---

### 1.6 Destruction

Always use the paired VMA destroy functions, not raw Vulkan:

```cpp
vmaDestroyBuffer(g_allocator, vkBuffer, allocation);  // do NOT call vkDestroyBuffer directly
vmaDestroyImage (g_allocator, vkImage,  allocation);  // do NOT call vkDestroyImage directly
```

Calling the raw Vulkan destroy functions directly would leak the `VmaAllocation` tracking entry and potentially leave the sub-allocated memory block un-recyclable.

---

### 1.7 Transient upload allocator and VMA

The `FrameContext` transient upload allocator (RHI.md §13.3) should be backed by a **persistently-mapped** `CpuToGpu` buffer allocated via VMA:

```cpp
VmaAllocationCreateInfo allocInfo{};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                | VMA_ALLOCATION_CREATE_MAPPED_BIT;  // persistent map — no map/unmap per frame

VmaAllocationInfo info;
vmaCreateBuffer(g_allocator, &bufInfo, &allocInfo, &ringBuffer, &ringAllocation, &info);
void* mappedPtr = info.pMappedData;  // write CPU constants here each frame
```

Persistent mapping avoids the overhead of `vkMapMemory` / `vkUnmapMemory` per frame. VMA guarantees this is safe as long as the memory type supports `HOST_COHERENT` or the caller flushes explicitly before submit.

---

### 1.8 Constraints and out-of-scope items

The following VMA features are **out of scope for v1**:

| Feature | Reason deferred |
|---------|-----------------|
| Defragmentation (`vmaBeginDefragmentation`) | Only beneficial for long-running scenes with heavy dynamic allocation; adds implementation complexity |
| Custom memory pools (`VmaPoolCreateInfo`) | Useful for aliasing or dedicated allocations; not needed until resource aliasing is a goal |
| Dedicated allocations for large resources | VMA handles this automatically via `VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT` heuristics; no explicit call needed in v1 |

---

## 2. Required Vulkan objects

For shader resource binding and pipelines, the Vulkan backend should maintain:

- `VkShaderModule`
- `VkDescriptorSetLayout` per set
- `VkPipelineLayout`
- `VkDescriptorSet` per `ResourceSet`
- `VkDescriptorPool` or descriptor allocator
- `VkPipeline`

---

## 3. Pipeline layout translation

`PipelineLayoutDesc` (RHI.md §7.3) is translated into:

- one `VkDescriptorSetLayout` per logical set index
- one `VkPipelineLayout` containing all descriptor set layouts and push constant ranges

---

## 4. ResourceSet translation

A `ResourceSet` (RHI.md §8) becomes:

- one `VkDescriptorSet`
- updated using `vkUpdateDescriptorSets`
- rebound through `vkCmdBindDescriptorSets`

If constant data is represented as a uniform buffer in the reflected layout, the engine must upload that data into a backend-owned buffer and bind it through the descriptor set.

---

## 5. Binding model

At draw time:

- `bindGraphicsPipeline()` → `vkCmdBindPipeline`
- `bindResourceSet(setIndex, set)` → `vkCmdBindDescriptorSets`
- `pushConstants()` → `vkCmdPushConstants`
- vertex/index bindings → `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer`

---

## 6. Barrier model

The Vulkan backend implements the `CommandList::textureBarrier()` / `bufferBarrier()` primitives (RHI.md §11.2) using `vkCmdPipelineBarrier2` (Vulkan 1.3 / `VK_KHR_synchronization2`).

`TextureState` / `BufferState` (RHI.md §11.4) map to `VkImageLayout` / access mask pairs as follows:

| `TextureState`     | `VkImageLayout`                              |
|--------------------|----------------------------------------------|
| `Undefined`        | `VK_IMAGE_LAYOUT_UNDEFINED`                  |
| `RenderTarget`     | `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`   |
| `DepthStencil`     | `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |
| `ShaderRead`       | `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`   |
| `ShaderReadWrite`  | `VK_IMAGE_LAYOUT_GENERAL`                    |
| `CopySource`       | `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`       |
| `CopyDest`         | `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`       |
| `Present`          | `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`            |

---

## 7. Swapchain

`Device::createSwapchain()` uses the `void* nativeWindowHandle` to create a `VkSurfaceKHR` via the appropriate platform extension:

- **Windows**: `VK_KHR_win32_surface` — handle is a `HWND`
- **macOS/iOS**: `VK_EXT_metal_surface` — handle is a `CAMetalLayer*`
- **Linux (Xlib)**: `VK_KHR_xlib_surface` — handle encodes a `Display*` and an Xlib `Window`
- **Linux (XCB)**: `VK_KHR_xcb_surface` — handle encodes an `xcb_connection_t*` and an `xcb_window_t`

These are separate Vulkan extensions with separate `VkSurfaceKHR` creation paths (`vkCreateXlibSurfaceKHR` vs. `vkCreateXcbSurfaceKHR`). Choose one per platform layer; do not mix them.

The swapchain itself is backed by `VkSwapchainKHR`. `acquireNextImage()` calls `vkAcquireNextImageKHR`; `present()` calls `vkQueuePresentKHR`.

---

## 8. Implementation guidance

Version 1 should avoid:

- descriptor buffer extensions
- bindless-first architecture
- overly abstract descriptor heap virtualization

Standard descriptor-set-based implementation is sufficient and most stable.
