# Metal Phase 1 — Platform-Selected Backend Wiring

This document defines the Phase 1 integration work required to make the Metal
backend buildable and runnable on macOS, while preserving the existing OpenGL
path on non-Apple platforms.

It is intentionally narrower than `metal_backend.md`: the Metal backend source
files already exist, and this document focuses only on wiring them into the
build, application startup, windowing path, and immediate runtime support.

**Related docs**:
- `metal_backend.md` — long-term Metal architecture and phased roadmap
- `shader_material_system.md` — shader resource model evolution
- `resource_packaging.md` — compiled shader packaging/runtime lookup

---

## 1. Decision Summary

### 1.1 Backend Selection Policy

Backend selection is **platform-defined at configure time**, not user-configured:

- **macOS** builds use **Metal only**
- **Windows/Linux** builds use **OpenGL only**

This replaces the earlier idea of a user-facing `GLAB_BACKEND_METAL` toggle.

### 1.2 Why Platform Selection

The current renderer and demos are developed around an OpenGL 4.6 path.
macOS does not provide OpenGL 4.6 and is effectively limited to OpenGL 4.1.
Maintaining a macOS OpenGL configuration during Metal bring-up would add
testing and codepath complexity without matching the actual target platform
capabilities.

For Phase 1, the cleanest policy is:

- macOS: Metal-only
- non-Apple: OpenGL-only

This keeps the build deterministic and avoids shipping a macOS OpenGL path that
is not representative of the intended renderer feature level.

### 1.3 Scope of This Phase

After this work:

- macOS configures and builds with Metal without requiring `OpenGL::GL` or `glad`
- the app creates a GLFW window using `GLFW_NO_API`
- `MetalGraphicsDevice` is created during application startup
- the frame loop runs without crashing
- presentation happens via `CAMetalLayer` / `MTLCommandBuffer`
- resize updates the Metal drawable size
- ImGui remains alive as a UI/input layer, but its renderer backend is disabled
  on Metal until the later Phase 4 integration

This phase does **not** aim to complete full feature parity or ImGui Metal
rendering.

---

## 2. Goals and Non-Goals

### 2.1 Goals

- Make backend selection deterministic by platform
- Compile Objective-C++ Metal files on macOS
- Remove OpenGL/glad as build requirements for the macOS path
- Keep non-Apple platforms on the current OpenGL path with no behavior changes
- Allow the app to open a window and present empty/cleared frames on macOS
- Ensure resize updates `CAMetalLayer.drawableSize`

### 2.2 Non-Goals

- Runtime backend switching
- User-visible backend CMake options
- ImGui Metal renderer integration
- Vulkan integration
- Refactoring the broader RHI architecture

---

## 3. Configure-Time Backend Model

The root CMake configuration defines two internal variables:

```cmake
if(APPLE)
    set(GLAB_BACKEND_METAL ON)
    set(GLAB_BACKEND_OPENGL OFF)
else()
    set(GLAB_BACKEND_METAL OFF)
    set(GLAB_BACKEND_OPENGL ON)
endif()
```

These are **internal build-routing variables**, not public options.

### 3.1 Consequences

- `if(GLAB_BACKEND_METAL)` means “this build is the macOS Metal configuration”
- `if(GLAB_BACKEND_OPENGL)` means “this build is the non-Apple OpenGL configuration”
- code and build files should branch on those variables rather than `APPLE`
  directly whenever the distinction is specifically about graphics backend

### 3.2 Rationale

This keeps future backend growth possible without reopening every call site.
If Vulkan is added later, the project can evolve toward a string backend
selector or a richer backend matrix, but Phase 1 does not need that machinery.

---

## 4. Required File Changes

Phase 1 touches these areas:

1. Root CMake backend selection and language support
2. Vendor CMake dependency routing for ImGui/OpenGL
3. Core source list and target link selection
4. Graphics device resize hook
5. Window creation and presentation path
6. Application device creation path
7. ImGui startup/render behavior on Metal
8. Shader compile target auto-selection

The concrete files are:

- `CMakeLists.txt`
- `vendor/CMakeLists.txt`
- `src/CMakeLists.txt`
- `src/graphics/interface/IGraphicsDevice.h`
- `src/graphics/metal/MetalGraphicsDevice.h`
- `src/graphics/metal/MetalGraphicsDevice.mm`
- `src/core/app/Application.cpp`
- `src/core/app/Window.cpp`
- `src/gui/ImGuiLayer.cpp`
- `cmake/CompileShaders.cmake`

---

## 5. Detailed Design

### 5.1 Root CMake: Platform-Selected Backend

**File**: `CMakeLists.txt`

#### Required changes

1. Add `OBJCXX` to the project languages so `.mm` files compile on macOS.
2. Define backend selection variables based on platform.
3. Make OpenGL package discovery conditional on the OpenGL backend.

#### Target shape

```cmake
project(RTRLab
    VERSION 0.1.0
    DESCRIPTION "A C++ rendering playground"
    LANGUAGES C CXX OBJCXX
)

if(APPLE)
    set(GLAB_BACKEND_METAL ON)
    set(GLAB_BACKEND_OPENGL OFF)
else()
    set(GLAB_BACKEND_METAL OFF)
    set(GLAB_BACKEND_OPENGL ON)
endif()

if(GLAB_BACKEND_OPENGL)
    find_package(OpenGL REQUIRED)
endif()
```

#### Notes

- `find_package(OpenGL REQUIRED)` must not run on the Metal path, or the macOS
  build still incorrectly depends on OpenGL at configure time.
- No public `option(GLAB_BACKEND_METAL ...)` is introduced in this design.

### 5.2 Vendor CMake: ImGui/OpenGL Dependency Routing

**File**: `vendor/CMakeLists.txt`

The current `imgui` target always builds:

- `imgui_impl_glfw.cpp`
- `imgui_impl_opengl3.cpp`

and always links:

- `glfw`
- `glad`

That does not work for the Metal path.

#### Required changes

1. Always compile the core ImGui sources and GLFW platform backend.
2. Compile the OpenGL3 renderer backend only for OpenGL builds.
3. Link `glad` only for OpenGL builds.

#### Target shape

```cmake
set(GLAB_IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_glfw.cpp
)

if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_IMGUI_SOURCES
        imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

add_library(imgui STATIC
    ${GLAB_IMGUI_SOURCES}
)

target_link_libraries(imgui PRIVATE
    glfw
)

if(GLAB_BACKEND_OPENGL)
    target_link_libraries(imgui PRIVATE
        glad
    )
endif()
```

#### Notes

- `imgui_impl_metal.mm` is deliberately out of scope for this phase.
- The GLFW backend remains useful on Metal for platform input integration even
  when no ImGui renderer backend is active yet.

### 5.3 Core CMake: Backend-Specific Sources and Links

**File**: `src/CMakeLists.txt`

The current target unconditionally includes OpenGL source files, OpenGL headers,
and links `glad` and `OpenGL::GL`.

#### Required changes

1. Move all `graphics/opengl/*` sources and headers into an OpenGL-only block.
2. Add all `graphics/metal/*` sources and headers in a Metal-only block.
3. Keep `.mm` files out of unity builds.
4. Link OpenGL dependencies only on the OpenGL path.
5. Link Metal/AppKit frameworks only on the Metal path.
6. Add a backend compile definition for the Metal path.
7. Ensure `source_group()` includes both backend-specific file lists.
8. Condition any existing GL-only `set_source_files_properties(...)` block on
   the OpenGL backend.

#### Design sketch

Keep the shared engine files in `GLAB_CORE_SOURCES` / `GLAB_CORE_HEADERS`, but
remove unconditional `graphics/opengl/*` entries from those base lists.

Then append backend-specific files:

```cmake
if(GLAB_BACKEND_OPENGL)
    list(APPEND GLAB_CORE_SOURCES
        graphics/opengl/GLTexture2D.cpp
        graphics/opengl/GLVertexBuffer.cpp
        graphics/opengl/GLIndexBuffer.cpp
        graphics/opengl/GLVertexArray.cpp
        graphics/opengl/GLShader.cpp
        graphics/opengl/GLFramebuffer.cpp
        graphics/opengl/GLRenderTarget.cpp
        graphics/opengl/GLRenderCommand.cpp
        graphics/opengl/GLGraphicsDevice.cpp
    )

    list(APPEND GLAB_CORE_HEADERS
        graphics/opengl/GLCast.h
        graphics/opengl/GLTexture2D.h
        graphics/opengl/GLVertexBuffer.h
        graphics/opengl/GLIndexBuffer.h
        graphics/opengl/GLVertexArray.h
        graphics/opengl/GLShader.h
        graphics/opengl/GLFramebuffer.h
        graphics/opengl/GLRenderTarget.h
        graphics/opengl/GLRenderCommand.h
        graphics/opengl/GLGraphicsDevice.h
    )
endif()

if(GLAB_BACKEND_METAL)
    set(GLAB_METAL_SOURCES
        graphics/metal/MetalGraphicsDevice.mm
        graphics/metal/MetalRenderCommand.mm
        graphics/metal/MetalShader.mm
        graphics/metal/MetalVertexBuffer.mm
        graphics/metal/MetalIndexBuffer.mm
        graphics/metal/MetalVertexArray.mm
        graphics/metal/MetalTexture2D.mm
        graphics/metal/MetalFramebuffer.mm
        graphics/metal/MetalRenderTarget.mm
    )

    set(GLAB_METAL_HEADERS
        graphics/metal/MetalCast.h
        graphics/metal/MetalTypes.h
        graphics/metal/MetalGraphicsDevice.h
        graphics/metal/MetalRenderCommand.h
        graphics/metal/MetalShader.h
        graphics/metal/MetalVertexBuffer.h
        graphics/metal/MetalIndexBuffer.h
        graphics/metal/MetalVertexArray.h
        graphics/metal/MetalTexture2D.h
        graphics/metal/MetalFramebuffer.h
        graphics/metal/MetalRenderTarget.h
    )

    set_source_files_properties(${GLAB_METAL_SOURCES}
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
endif()
```

After `add_library(RTRLabCore STATIC ...)`:

```cmake
if(GLAB_BACKEND_OPENGL)
    target_link_libraries(RTRLabCore PUBLIC
        glad
        OpenGL::GL
    )
endif()

if(GLAB_BACKEND_METAL)
    target_sources(RTRLabCore PRIVATE
        ${GLAB_METAL_SOURCES}
        ${GLAB_METAL_HEADERS}
    )

    target_link_libraries(RTRLabCore PUBLIC
        "-framework Metal"
        "-framework QuartzCore"
        "-framework AppKit"
    )

    target_compile_definitions(RTRLabCore PUBLIC
        GLAB_BACKEND_METAL
    )
endif()
```

#### Notes

- `AppKit` is preferred here because the implementation imports
  `<AppKit/AppKit.h>`.
- Shared libraries like `glfw`, `glm`, `stb`, `spdlog`, `imgui`,
  and `nlohmann_json` remain linked for both backends.

### 5.4 Graphics Device Resize Hook

**Files**:
- `src/graphics/interface/IGraphicsDevice.h`
- `src/graphics/metal/MetalGraphicsDevice.h`
- `src/graphics/metal/MetalGraphicsDevice.mm`
- `src/core/app/Application.cpp`

The current Metal device initializes `CAMetalLayer.drawableSize` once during
construction. That is insufficient after window/framebuffer resize.

#### Required changes

Add a default no-op resize hook to the graphics device interface:

```cpp
virtual void OnResize(uint32_t width, uint32_t height) {}
```

Override it in `MetalGraphicsDevice`:

```cpp
void OnResize(uint32_t width, uint32_t height) override;
```

Implementation:

```cpp
void MetalGraphicsDevice::OnResize(uint32_t width, uint32_t height)
{
    m_Impl->layer.drawableSize = CGSizeMake(width, height);
}
```

Then call it from `Application::OnWindowResize()` after the viewport update:

```cpp
RenderCommand::SetViewport(0, 0, width, height);
GetDevice()->OnResize(width, height);
```

#### Why the hook lives on `IGraphicsDevice`

- the device owns the native layer/swapchain-ish integration details
- `Window` should stay platform-window focused, not renderer-resource focused
- the OpenGL path remains unchanged because the default implementation is a no-op

### 5.5 Application Startup: Backend-Specific Device Creation

**File**: `src/core/app/Application.cpp`

The current startup path unconditionally includes and constructs
`GLGraphicsDevice`.

#### Required changes

1. Make backend includes conditional.
2. Create the appropriate device based on the compile-time backend define.
3. Keep the rest of startup unchanged.

#### Target shape

```cpp
#ifdef GLAB_BACKEND_METAL
#include "graphics/metal/MetalGraphicsDevice.h"
#else
#include "graphics/opengl/GLGraphicsDevice.h"
#endif
```

Constructor path:

```cpp
#ifdef GLAB_BACKEND_METAL
    SetDevice(CreateRef<MetalGraphicsDevice>(m_Window->GetNativeHandle()));
#else
    SetDevice(CreateRef<GLGraphicsDevice>());
#endif
```

Resize path:

```cpp
RenderCommand::SetViewport(0, 0, width, height);
GetDevice()->OnResize(width, height);
```

#### Notes

- The `BeginFrame()` / `EndFrame()` bracketing already exists and does not need
  structural changes for this phase.
- The `GLGraphicsDevice.h` include must not remain unconditional, or the
  Metal-only macOS build still drags in OpenGL headers.

### 5.6 Window: GLFW_NO_API and GL Guarding

**File**: `src/core/app/Window.cpp`

The current implementation always creates an OpenGL context, loads GL via Glad,
registers a GL debug callback, swaps via GLFW, and sets VSync through
`glfwSwapInterval()`.

That must be split by backend.

#### Required changes

1. Guard `#include <glad/glad.h>` behind `#ifndef GLAB_BACKEND_METAL`.
2. Use `GLFW_NO_API` on the Metal path.
3. Skip `glfwMakeContextCurrent()` on Metal.
4. Skip `gladLoadGLLoader()` on Metal.
5. Skip all GL debug callback setup on Metal.
6. Skip GL vendor/renderer/version logging on Metal.
7. Make `SwapBuffers()` a no-op on Metal.
8. Make `SetVSync()` avoid `glfwSwapInterval()` on Metal.

#### Target shape

Window hints:

```cpp
#ifdef GLAB_BACKEND_METAL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
```

Context/GL init:

```cpp
#ifndef GLAB_BACKEND_METAL
    glfwMakeContextCurrent(m_Handle);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD.");

    // GL debug callback setup
    // GL vendor/renderer/version logging
#endif
```

Swap:

```cpp
void Window::SwapBuffers()
{
#ifndef GLAB_BACKEND_METAL
    glfwSwapBuffers(m_Handle);
#endif
}
```

VSync:

```cpp
void Window::SetVSync(bool enabled)
{
#ifndef GLAB_BACKEND_METAL
    glfwSwapInterval(enabled ? 1 : 0);
#endif
    m_VSync = enabled;
}
```

#### Notes

- The OpenGL debug callback block must be guarded together with the GL init;
  leaving that block outside the guard will fail to compile on the Metal path.
- Because backend selection is already platform-defined in root CMake, the
  OpenGL branch here is the non-Apple OpenGL path. No Apple-specific OpenGL
  hint branch is needed in this design.
- Presentation on Metal happens in `MetalRenderCommand::EndFrame()` via
  `presentDrawable`, not via `glfwSwapBuffers()`.

### 5.7 ImGui: Keep Platform Backend, Disable GL Renderer on Metal

**File**: `src/gui/ImGuiLayer.cpp`

The current ImGui layer always initializes:

- the GLFW platform backend using the OpenGL-oriented entry point
- the OpenGL3 renderer backend

That assumes a GL context exists.

#### Required changes

1. Include `imgui_impl_opengl3.h` only on the OpenGL backend.
2. On Metal, initialize the GLFW backend with `ImGui_ImplGlfw_InitForOther`.
3. Skip OpenGL renderer init/shutdown/frame/render calls on Metal.
4. Keep ImGui context creation, style setup, capture flags, and GLFW frame
   handling active on both backends.

#### Target shape

```cpp
#include <imgui.h>
#include <imgui_impl_glfw.h>
#ifndef GLAB_BACKEND_METAL
#include <imgui_impl_opengl3.h>
#endif
```

Attach:

```cpp
#ifdef GLAB_BACKEND_METAL
    ImGui_ImplGlfw_InitForOther(window, true);
#else
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
#endif
```

Detach:

```cpp
#ifndef GLAB_BACKEND_METAL
    ImGui_ImplOpenGL3_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
```

Begin:

```cpp
#ifndef GLAB_BACKEND_METAL
    ImGui_ImplOpenGL3_NewFrame();
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
```

End:

```cpp
    ImGui::Render();
#ifndef GLAB_BACKEND_METAL
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
```

#### Expected behavior in Phase 1

- ImGui widgets can still be built by the app
- input capture and UI interaction state remain valid
- no ImGui draw data is submitted to GPU on Metal yet
- this is acceptable because ImGui Metal rendering is explicitly a later-phase task

### 5.8 Shader Compilation: Auto-Enable Metal Target

**File**: `cmake/CompileShaders.cmake`

Shader output targets should follow the selected backend. In this phase:

- Metal builds emit **MSL only**
- OpenGL builds emit **GLSL only**

#### Required change

Before the per-shader loop:

```cmake
if(GLAB_BACKEND_METAL)
    set(GLAB_SHADER_TARGET_METAL ON)
    set(GLAB_SHADER_TARGET_GLSL OFF)
    set(GLAB_SHADER_TARGET_SPIRV OFF)
endif()

if(GLAB_BACKEND_OPENGL)
    set(GLAB_SHADER_TARGET_GLSL ON)
    set(GLAB_SHADER_TARGET_METAL OFF)
    set(GLAB_SHADER_TARGET_SPIRV OFF)
endif()
```

#### Notes

- This keeps shader outputs aligned with the runtime backend.
- macOS should not spend build time producing GLSL that is never consumed by the
  Metal runtime path.
- non-Apple OpenGL builds likewise do not need MSL output in this phase.
- No user flag is required in this design.

---

## 6. Invariants and Constraints

These rules should remain true after the implementation:

- macOS build does not require `glad`
- macOS build does not require `find_package(OpenGL)`
- macOS build does not compile any `graphics/opengl/*` translation units
- macOS build does not compile `imgui_impl_opengl3.cpp`
- macOS build does not link `OpenGL::GL`
- macOS build does not create an OpenGL context or call GL loader/setup code
- macOS build does not emit GLSL shader outputs
- non-Apple builds do not compile Objective-C++ Metal sources
- non-Apple builds preserve the existing OpenGL renderer behavior
- Metal path does not call GLFW functions that require a current GL context
- resize updates both engine viewport state and the Metal drawable size

### 6.1 Apple-Specific Prohibition List

When `APPLE` is true, the resulting build must not do any of the following:

- run `find_package(OpenGL REQUIRED)`
- add `graphics/opengl/*.cpp` or `graphics/opengl/*.h` to `RTRLabCore`
- link `glad` into `RTRLabCore`
- link `OpenGL::GL` into `RTRLabCore`
- compile `imgui_impl_opengl3.cpp`
- emit GLSL shader outputs for runtime use
- include `graphics/opengl/GLGraphicsDevice.h` from `Application.cpp`
- include `<glad/glad.h>` from `Window.cpp`
- call `glfwMakeContextCurrent()`
- call `gladLoadGLLoader()`
- install the OpenGL debug callback
- log OpenGL vendor/renderer/version strings
- call `glfwSwapBuffers()` as the presentation path
- call `glfwSwapInterval()` under the assumption that a GL context exists

The only graphics backend expected on Apple in this phase is Metal.

---

## 7. Verification Plan

### 7.1 macOS / Metal

Configure and build:

```bash
cmake -B build/metal -G Ninja
cmake --build build/metal
```

Run:

```bash
./build/metal/bin/Debug/RTRLab
```

Expected results:

- project configures without discovering/linking OpenGL as a required backend dependency
- all `.mm` files compile successfully
- application opens a window
- frame loop runs without crash
- Metal validation reports no immediate presentation/setup errors
- resizing the window does not produce stretched backbuffer sizing or a crash

### 7.2 Windows/Linux / OpenGL

Configure and build:

```bash
cmake -B build/gl -G Ninja
cmake --build build/gl
```

Expected results:

- project configures with OpenGL as before
- existing demos continue to run
- ImGui OpenGL rendering continues to work
- no behavior regression from backend routing changes

### 7.3 Acceptance Criteria

This phase is complete when:

- macOS produces a stable Metal windowed build
- non-Apple platforms still produce the existing OpenGL build
- no OpenGL-only compile/link dependencies remain in the macOS path
- ImGui no longer blocks Metal startup even though Metal rendering for ImGui is
  still postponed

---

## 8. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Leaving `find_package(OpenGL REQUIRED)` unconditional | Metal build still depends on OpenGL at configure time | Guard package discovery with `GLAB_BACKEND_OPENGL` |
| Leaving `GLGraphicsDevice.h` unconditionally included in `Application.cpp` | Metal build fails at compile time | Make backend includes mutually exclusive |
| Guarding GL init but not the GL debug callback block | Metal build fails due to unresolved GL symbols | Wrap the entire GL-only setup path |
| Forgetting to route ImGui source selection in vendor CMake | Metal build still compiles OpenGL ImGui renderer | Make `imgui_impl_opengl3.cpp` conditional |
| Resize only updates engine viewport, not `drawableSize` | Stretched or mismatched backbuffer on Metal | Add `IGraphicsDevice::OnResize()` hook |
| Treating `glfwSwapBuffers()` as universal | No-op/incorrect present model on Metal | Presentation remains exclusively in `MetalRenderCommand::EndFrame()` |

---

## 9. Implementation Checklist

### Root CMake

- Add `OBJCXX` to `project(... LANGUAGES ...)`
- Define `GLAB_BACKEND_METAL` / `GLAB_BACKEND_OPENGL` from platform
- Guard `find_package(OpenGL REQUIRED)`

### Vendor

- Split ImGui GLFW platform backend from OpenGL renderer backend
- Remove unconditional `glad` link from `imgui`

### Core Build

- Move OpenGL sources/headers into backend-conditional lists
- Add Metal sources/headers on macOS
- Guard GL-only source properties
- Add Metal framework links
- Add `GLAB_BACKEND_METAL` compile definition
- Update `source_group()`

### Runtime

- Add `IGraphicsDevice::OnResize()`
- Implement `MetalGraphicsDevice::OnResize()`
- Call `GetDevice()->OnResize()` in `Application::OnWindowResize()`
- Create `MetalGraphicsDevice` in `Application.cpp` on Metal builds
- Guard all GL-only code in `Window.cpp`
- Make ImGui use `InitForOther()` and skip renderer calls on Metal

### Shader Build

- Auto-enable `GLAB_SHADER_TARGET_METAL` when Metal backend is active
- Disable non-Metal shader targets on the Metal path

---

## 10. Final Revised Plan

### Phase 1 Wiring Plan

1. Update root CMake so backend selection is platform-defined, Objective-C++ is
   enabled, and OpenGL is discovered only for non-Apple builds.
2. Update vendor CMake so ImGui always builds the GLFW platform backend, but
   only builds and links the OpenGL renderer backend on the OpenGL path.
3. Refactor `src/CMakeLists.txt` so OpenGL and Metal source/header sets are
   backend-conditional, and Metal frameworks are linked only on the Metal path.
4. Add a no-op resize hook to `IGraphicsDevice`, implement it in
   `MetalGraphicsDevice`, and call it from `Application::OnWindowResize()`.
5. Update `Application.cpp` so backend-specific includes and device creation are
   selected by compile-time backend.
6. Update `Window.cpp` so Metal uses `GLFW_NO_API`, never creates a GL context,
   never loads Glad, never installs GL debug callbacks, and never swaps via GLFW.
7. Update `ImGuiLayer.cpp` so Metal uses the GLFW platform backend without the
   OpenGL renderer backend, preventing startup crashes while deferring actual
   ImGui Metal rendering to a later phase.
8. Update shader compilation so the Metal backend automatically emits MSL output.

### Deliverable

At the end of this phase:

- macOS builds and runs the Metal backend
- non-Apple platforms continue to build and run the OpenGL backend
- the repository has a single, self-consistent backend selection policy
- Metal backend bring-up is no longer blocked by the build system or app startup path
