# Real Time Rendering Lab

A modern C++ / OpenGL graphics playground for experimenting with real-time rendering techniques and graphics algorithms.

This repository is designed as a long-term platform for graphics experiments, learning, and visualization.
It provides a lightweight framework where different rendering techniques can be implemented and explored as independent demos.

[中文文档](./docs/zh-CN/README.zh-CN.md)

---

## Features

- **Demo Framework** — modular architecture where each rendering technique lives as an independent, hot-switchable demo
- **Multi-Backend Graphics Abstraction** — pure virtual interfaces (`IShader`, `ITexture2D`, `IFramebuffer`, etc.) with OpenGL backend; designed for Metal/Vulkan extension
- **SPIR-V Shader Pipeline** — GLSL source → SPIR-V (glslang, build time) → backend GLSL (SPIRV-Cross, runtime); single source, multiple backends
- **Forward Rendering Pipeline** — multi-pass renderer with shadow mapping support
- **Blinn-Phong Shading** — ambient + diffuse + specular lighting with directional light
- **Shadow Mapping** — directional light depth pass with front face culling, slope-scaled bias, and 3x3 PCF soft shadows
- **Material System** — pass-centric materials with reflection-driven uniform packing and logical resource binding
- **Procedural Meshes** — built-in cube, plane, fullscreen quad, and UV sphere generators
- **ImGui Integration** — debug panels for framerate, memory, and demo selection
- **First-Person Camera** — WASD + mouse look with scroll-wheel FOV control
- **Framebuffer Abstraction** — off-screen rendering with multiple color/depth attachments and pixel read-back
- **DSA OpenGL** — modern Direct State Access style for shaders, buffers, textures, and framebuffers

---

## Demo Gallery

| Demo | Description |
|------|-------------|
| **[Shadow Mapping](./docs/en/demos/shadow-mapping.md)** | Blinn-Phong shading + directional light shadow mapping with 3x3 PCF, front face culling, and debug visualization |
| **[Material Playground](./docs/en/demos/material-playground.md)** | 5 UV spheres with distinct Blinn-Phong presets, per-sphere albedo / specular / ambient editing via ImGui |

![Shadow Mapping](./docs/screenshots/ShadowMapping.png)
![Material Playground](./docs/screenshots/MaterialPlayground.png)

---

## Project Structure

```
RT_Rendering_Lab/
├── src/
│   ├── core/           # Core runtime utilities and systems
│   │   ├── app/        # Application, Window, Layer, LayerStack
│   │   ├── event/      # EventBus, Events, ScopedConnection
│   │   └── input/      # Input, InputAction, KeyCode, MouseCode
│   ├── graphics/       # Abstract interfaces (interface/I*.h), OpenGL backend (opengl/GL*), Mesh, Material
│   ├── renderer/       # SceneRenderer, ForwardPass, ShadowPass, TexturePreviewPass
│   ├── scene/          # Camera, DebugCameraController, Light, Transform, SceneData
│   ├── demos/          # DemoBase, DemoRegistry, LabLayer, ShadowMapping/, MaterialPlayground/
│   ├── gui/            # ImGuiLayer, DebugPanel, DemoSelectorPanel
│   ├── Tools/          # Developer utilities such as source catalog indexing
│   └── main.cpp
├── Content/
│   ├── .rtr/           # Generated source catalogs for loose content mounts
│   ├── shaders/        # GLSL source (.vert/.frag) + compiled SPIR-V (.spv)
│   ├── models/
│   ├── textures/
│   └── scenes/
├── tests/
│   ├── unit/           # Time, LayerStack, Transform, Camera, Buffers, CameraController
│   ├── integration/    # Shader, Texture, Framebuffer, RenderTarget (require OpenGL context)
│   └── support/        # GLTestContext, MathTestUtils, TestLayer
├── docs/
│   └── roadmap.md
├── vendor/             # Third-party: GLFW, GLM, ImGui, glslang, SPIRV-Cross (submodules), Glad, STB
└── CMakeLists.txt
```

---

## Building

### Requirements

- C++20 compatible compiler (MSVC, GCC, Clang)
- CMake 3.20+
- OpenGL 4.6 capable GPU & driver

All other dependencies are included in `vendor/` or fetched automatically by CMake.

### Clone

```bash
git clone --recursive <repository-url>
cd RT_Rendering_Lab
```

> If you already cloned without `--recursive`, run `git submodule update --init --recursive`.

Current pinned submodule versions:

- `GLFW`: `3.4.0`
- `GLM`: `1.0.3`
- `Dear ImGui`: `1.92.6` (`docking` branch)
- `glslang`: `vulkan-sdk-1.4.304.1`
- `SPIRV-Cross`: `vulkan-sdk-1.4.304.1`

### Build with Presets

The project ships with CMake presets for common configurations:

```bash
# Visual Studio 2026 — Debug
cmake --preset windows-vs-debug
cmake --build build/windows-vs-debug

# Visual Studio 2026 — Fast debug iteration
cmake --preset windows-vs-debug-fast
cmake --build --preset build-windows-debug-fast --parallel

# Ninja — Release
cmake --preset ninja-release
cmake --build build/ninja-release

# Ninja — Fast debug iteration (requires Ninja installed)
cmake --preset ninja-debug-fast
cmake --build --preset build-ninja-debug-fast
```

`windows-vs-debug-fast` and `ninja-debug-fast` are tuned for shorter edit-build-run loops:

- tests disabled (`GLAB_BUILD_TESTS=OFF`)
- unity build enabled (`GLAB_ENABLE_UNITY_BUILD=ON`)
- precompiled headers enabled (`GLAB_ENABLE_PCH=ON`)
- Dear ImGui demo translation unit skipped (`GLAB_BUILD_IMGUI_DEMO=OFF`)

The unity build keeps OpenGL / GLFW-heavy translation units out of unity batches to avoid header-order conflicts while still speeding up most of the project.

### Build Manually

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/<config>/RTRLab      # main application
```

---

## Testing

The project uses **Google Test** (fetched automatically via CMake FetchContent).

```bash
# Build with tests (enabled by default)
cmake --preset windows-vs-debug
cmake --build build/windows-vs-debug

# Run tests
ctest --test-dir build/windows-vs-debug
```

Test executables:

- `rtrlab_unit_tests`
- `rtrlab_contract_tests`
- `rtrlab_contract_tests_opengl`
- `rtrlab_integration_tests_opengl`

Resource tooling:

- `rtr_asset_index` generates `.rtr/catalog.json` for `Content/`,
  `EngineContent/`, and `Plugins/*/Content/`
- `rtr_asset_cook` writes loose cooked `.rtr/catalog.json` files under
  `Saved/Cache/Cooked/`; texture assets currently decode to an RGBA8 bootstrap
  binary at cooked `.ktx2` artifact paths, and the runtime can load/validate that
  bootstrap cooked texture payload through `Resource::LoadCookedTexture()`

```bash
./build/<config>/rtr_asset_index --root .
./build/<config>/rtr_asset_cook --root .
```

OpenGL contract and integration tests create a hidden OpenGL context — they require a GPU or software renderer.

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `GLAB_COMPILE_SHADERS` | `ON` | Compile GLSL shaders to SPIR-V at build time (requires glslang submodule) |
| `GLAB_BUILD_TESTS` | `ON` | Build the test suite |
| `GLAB_ENABLE_WARNINGS` | `ON` | Enable strict compiler warnings |
| `GLAB_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer (non-MSVC) |
| `GLAB_ENABLE_PCH` | `ON` | Enable precompiled headers for local C++ targets |
| `GLAB_ENABLE_UNITY_BUILD` | `OFF` | Merge selected `.cpp` files into unity batches for faster full builds |
| `GLAB_ENABLE_MSVC_MP` | `ON` | Enable MSVC multi-processor compilation (`/MP`) |
| `GLAB_BUILD_IMGUI_DEMO` | `OFF` | Compile `imgui_demo.cpp` into the Dear ImGui static library |

---

## Dependencies

| Library | Purpose | Source |
|---------|---------|--------|
| [GLFW](https://github.com/glfw/glfw) | Windowing & input | Git submodule (`vendor/glfw`, `3.4.0`) |
| [GLM](https://github.com/g-truc/glm) | Linear algebra | Git submodule (`vendor/glm`, `1.0.3`) |
| [Glad](https://glad.dav1d.de/) | OpenGL 4.6 loader | `vendor/glad/` |
| [STB Image](https://github.com/nothings/stb) | Image loading | `vendor/stb/` |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug GUI | Git submodule (`vendor/imgui`, `1.92.6`, `docking` branch) |
| [glslang](https://github.com/KhronosGroup/glslang) | GLSL to SPIR-V compiler | Git submodule (`vendor/glslang`, `vulkan-sdk-1.4.304.1`) |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | SPIR-V to GLSL transpiler | Git submodule (`vendor/spirv-cross`, `vulkan-sdk-1.4.304.1`) |
| [spdlog](https://github.com/gabime/spdlog) | Logging | `vendor/spdlog/` |
| [Google Test](https://github.com/google/googletest) | Testing framework | CMake FetchContent |

---

## Long-Term Direction

The project will gradually explore topics including:

- Modern real-time rendering (deferred, HDR, tone mapping)
- Physically based shading (Cook-Torrance, IBL)
- Screen-space techniques (SSAO, SSR, motion blur)
- GPU-driven rendering (compute shaders, indirect draw)
- Procedural generation (terrain, voxels)
- Ray tracing experiments (path tracing, hybrid rendering)

See [docs/en/roadmap.md](./docs/en/roadmap.md) for the full development plan.
