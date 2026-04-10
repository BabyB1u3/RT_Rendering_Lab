# Real Time Rendering Lab

A modern C++ graphics engine built for experimenting with real-time rendering techniques. The long-term goal is a multi-backend renderer (Vulkan, Metal, OpenGL) driven by **Slang** shaders and a modern explicit RHI, paired with a suite of rendering demos covering PBR, shadows, screen-space effects, and beyond.

[中文文档](./Docs/ZH_CN/README.zh-CN.md)

---

## Current Status

The project is in a **major architectural refactor**. The render system is being redesigned from scratch; render-dependent code from the previous iteration has been moved to `Archive/`. Core engine systems are active and continue to receive work.

| Area                              | Status                                  |
| --------------------------------- | --------------------------------------- |
| Core - App, Layer, Event          | Active                                  |
| Core - Input system               | Active                                  |
| Core - Resource system            | Active                                  |
| Core - Diagnostics, Serialization | Active                                  |
| Scene system                      | Active                                  |
| GUI / ImGui integration           | Active                                  |
| **RenderSystem**                  | **Offline - full redesign in progress** |

---

## Architecture Direction

The new render system is designed around a modern explicit RHI. Design documents live in [Docs/Modules/RenderSystem/Design/](./Docs/Modules/RenderSystem/Design/RHI.md).

Key decisions:

- **Slang** as the shader language - single source compiled per-backend, replacing the old GLSL - SPIR-V pipeline
- **Three backends: Vulkan · Metal · OpenGL** - Vulkan and Metal are primary targets; OpenGL is a compatibility backend
- **Vulkan-style public API** - explicit objects (`ShaderProgram`, `PipelineLayout`, `ResourceSet`, `GraphicsPipeline`, `CommandList`); no implicit global state
- **Reflection-driven parameter binding** - Slang reflection is the single source of truth for all shader parameter layout; no hand-maintained binding tables in C++

Render demos will be added once the new system reaches a usable state.

---

## Project Structure

```
RT_Rendering_Lab/
├── Src/
|  ├── Core/
|  |  ├── App/            # Application, Window, Layer, LayerStack
|  |  ├── Event/          # EventBus, Events, ScopedConnection
|  |  ├── Input/          # InputAction, KeyCode, MouseCode, replay
|  |  ├── Resource/       # Logical paths, catalog, cook, pack, mount, IO
|  |  ├── Diagnostics/    # Logging, Assert, Crash
|  |  └── Serialization/
|  ├── GUI/
|  |  ├── Backends/Metal/
|  |  └── Panels/
|  ├── Scene/              # Camera, Light, Transform, SceneData
|  ├── Demos/
|  |  └── 01_HelloWindow/
|  ├── Tools/              # Asset indexing, cooking, packaging
|  └── main.cpp
|  ├── Archive/                # Retired code from the previous renderer iteration
|  ├── graphics/           # Old IShader / ITexture2D / IFramebuffer + OpenGL backend
|  ├── renderer/           # Old SceneRenderer, ForwardPass, ShadowPass
|  └── shaders/            # Old GLSL source + SPIR-V
|  ├── Docs/
|  ├── Modules/
|  |  ├── RenderSystem/Design/   # RHI.md, ShaderSystem.md, backend docs
|  |  ├── ResourceSystem/
|  |  ├── DiagnosticsSystem/
|  |  ├── SerializationSystem/
|  |  └── InputEventSystem/
|  ├── EN/
|  └── ZH_CN/
|  ├── Content/                # Runtime assets (models, textures, scenes)
├── Tests/
|  ├── Unit/
|  ├── Integration/
|  └── Support/
├── Vendor/                 # GLFW, GLM, ImGui, glslang, SPIRV-Cross, Glad, STB, spdlog
└── CMakeLists.txt
```

---

## Building

### Requirements

- C++20 compatible compiler (MSVC, GCC, Clang)
- CMake 3.20+
- OpenGL 4.6 capable GPU & driver (for OpenGL-dependent tests)

All other dependencies are included in `Vendor/` or fetched automatically by CMake.

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

```bash
# Visual Studio 2026 - Debug
cmake --preset windows-vs-debug
cmake --build --preset build-windows-vs-debug

# Visual Studio 2026 - Fast debug iteration
cmake --preset windows-vs-debug-fast
cmake --build --preset build-windows-vs-debug-fast --parallel

# Ninja - Release
cmake --preset linux-ninja-release
cmake --build --preset build-linux-ninja-release

# Ninja - Fast debug iteration
cmake --preset linux-ninja-debug-fast
cmake --build --preset build-linux-ninja-debug-fast

# macOS Ninja - Release
cmake --preset macos-ninja-release
cmake --build --preset build-macos-ninja-release

# macOS Ninja - Fast debug iteration
cmake --preset macos-ninja-debug-fast
cmake --build --preset build-macos-ninja-debug-fast
```

`windows-vs-debug-fast`, `linux-ninja-debug-fast`, and `macos-ninja-debug-fast` disable tests, enable unity build and precompiled headers, and skip `imgui_demo.cpp` - tuned for shorter edit-build-run loops.

### Build Manually

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/<config>/RTRLab
```

---

## Testing

The project uses **Google Test** (fetched automatically via CMake FetchContent).

```bash
cmake --preset windows-vs-debug
cmake --build --preset build-windows-vs-debug
ctest --preset test-windows-vs-debug
```

Test executables: `rtrlab_unit_tests`, `rtrlab_integration_tests`.

### CMake Options

| Option                    | Default | Description                                                 |
| ------------------------- | ------- | ----------------------------------------------------------- |
| `GLAB_COMPILE_SHADERS`    | `ON`    | Compile GLSL shaders to SPIR-V at build time                |
| `GLAB_BUILD_TESTS`        | `ON`    | Build the test suite                                        |
| `GLAB_ENABLE_WARNINGS`    | `ON`    | Enable strict compiler warnings                             |
| `GLAB_ENABLE_ASAN`        | `OFF`   | Enable AddressSanitizer (non-MSVC)                          |
| `GLAB_ENABLE_PCH`         | `ON`    | Enable precompiled headers                                  |
| `GLAB_ENABLE_UNITY_BUILD` | `OFF`   | Merge selected `.cpp` files into unity batches              |
| `GLAB_ENABLE_MSVC_MP`     | `ON`    | Enable MSVC multi-processor compilation (`/MP`)             |
| `GLAB_BUILD_IMGUI_DEMO`   | `OFF`   | Compile `imgui_demo.cpp` into the Dear ImGui static library |

---

## Dependencies

| Library                                                    | Purpose                    | Source                                                       |
| ---------------------------------------------------------- | -------------------------- | ------------------------------------------------------------ |
| [GLFW](https://github.com/glfw/glfw)                       | Windowing & input          | Git submodule (`Vendor/glfw`, `3.4.0`)                       |
| [GLM](https://github.com/g-truc/glm)                       | Linear algebra             | Git submodule (`Vendor/glm`, `1.0.3`)                        |
| [Glad](https://glad.dav1d.de/)                             | OpenGL 4.6 loader          | `Vendor/glad/`                                               |
| [STB Image](https://github.com/nothings/stb)               | Image loading              | `Vendor/stb/`                                                |
| [Dear ImGui](https://github.com/ocornut/imgui)             | Debug GUI                  | Git submodule (`Vendor/imgui`, `1.92.6`, `docking` branch)   |
| [glslang](https://github.com/KhronosGroup/glslang)         | GLSL to SPIR-V compiler    | Git submodule (`Vendor/glslang`, `vulkan-sdk-1.4.304.1`)     |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | SPIR-V to GLSL transpiler  | Git submodule (`Vendor/spirv-cross`, `vulkan-sdk-1.4.304.1`) |
| [spdlog](https://github.com/gabime/spdlog)                 | Logging                    | `Vendor/spdlog/`                                             |
| [Google Test](https://github.com/google/googletest)        | Testing framework          | CMake FetchContent                                           |
| Slang                                                      | Shader language & compiler | Planned                                                      |

---

## Long-Term Direction

The immediate priority is completing the new RenderSystem - RHI layer, Slang integration, and at least one backend (Metal or Vulkan) to a point where render demos can run again.

Beyond that, the project will explore:

- Physically based shading (Cook-Torrance, IBL)
- Modern real-time rendering (deferred, HDR, tone mapping)
- Screen-space techniques (SSAO, SSR, motion blur)
- GPU-driven rendering (compute shaders, indirect draw)
- Procedural generation (terrain, voxels)
- Ray tracing experiments (path tracing, hybrid rendering)

See [Docs/EN/roadmap.md](./Docs/EN/roadmap.md) for the full development plan.

