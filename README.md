# Real Time Rendering Lab

A modern C++ graphics engine built for experimenting with real-time rendering techniques. The long-term goal is a multi-backend renderer (Vulkan, Metal, OpenGL) driven by **Slang** shaders and a modern explicit RHI, paired with a suite of rendering demos covering PBR, shadows, screen-space effects, and beyond.

[中文文档](./Docs/ZH_CN/README.ZH_CN.md)

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

```text
RT_Rendering_Lab/
├─ .github/                 # GitHub Actions workflows and PR metadata
├─ Archive/                 # Retired code from the previous renderer iteration
├─ CMake/                   # Shared CMake modules, presets, test helpers
├─ Docs/
│  ├─ EN/
│  ├─ Modules/              # Design, internals, plans, code reviews
│  └─ ZH_CN/
├─ Engine/                  # Engine-side source assets and config content
├─ Project/                 # Project-side source assets and config content
├─ Src/
│  ├─ Core/
│  │  ├─ App/               # Application, Window, Layer, LayerStack
│  │  ├─ Diagnostics/       # Assert, crash handling, logging
│  │  ├─ Event/             # EventBus, events, scoped connections
│  │  ├─ Input/             # Actions, devices, codes, replay
│  │  ├─ Resource/          # Catalog, cook, IO, mount, package, path
│  │  ├─ Serialization/     # PropertyTree, JSON backend, traits
│  │  └─ Util/              # Base helpers, command line, time
│  ├─ Demos/
│  │  └─ 01_HelloWindow/
│  ├─ GUI/
│  │  ├─ Backends/
│  │  └─ Panels/
│  ├─ Scene/                # Camera, controller, transform, scene data
│  ├─ Tools/                # `rtr_asset_cook`, `rtr_asset_pack`
│  ├─ main.cpp
│  └─ pch.h
├─ Tests/
│  ├─ Integration/
│  ├─ Smoke/
│  ├─ Support/
│  ├─ Unit/
│  ├─ CMakeLists.txt
│  └─ pch.h
├─ Tools/                   # Repository-level helper scripts and utilities
├─ Vendor/                  # Third-party dependencies
├─ .rtrproject              # Project marker file
├─ CMakeLists.txt
├─ CMakePresets.json
└─ README.md
```

---

## Building

### Requirements

- C++20 compatible compiler (MSVC, GCC, Clang)
- CMake 3.20+
- Ninja
- On Windows, run CMake from a Developer PowerShell / Developer Command Prompt so MSVC tools are on `PATH`
- On Linux, install the windowing dependencies used by GLFW and the CI matrix (`libgl-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libxkbcommon-dev`, `libwayland-dev`)
- On macOS, install Ninja with `brew install ninja`

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

### Build with Presets

```bash
# Windows - Debug
cmake --preset windows-debug
cmake --build --preset build-windows-debug --parallel

# Windows - RelWithDebInfo
cmake --preset windows-relwithdebinfo
cmake --build --preset build-windows-relwithdebinfo --parallel

# Windows - Release
cmake --preset windows-release
cmake --build --preset build-windows-release --parallel

# Windows - Release with tests enabled
cmake --preset windows-release-with-tests
cmake --build --preset build-windows-release-with-tests --parallel

# Linux - Debug
cmake --preset linux-debug
cmake --build --preset build-linux-debug --parallel

# Linux - Release
cmake --preset linux-release
cmake --build --preset build-linux-release --parallel

# Linux - Release with tests enabled
cmake --preset linux-release-with-tests
cmake --build --preset build-linux-release-with-tests --parallel

# macOS - Debug
cmake --preset macos-debug
cmake --build --preset build-macos-debug --parallel

# macOS - Release
cmake --preset macos-release
cmake --build --preset build-macos-release --parallel

# macOS - Release with tests enabled
cmake --preset macos-release-with-tests
cmake --build --preset build-macos-release-with-tests --parallel
```

All shipped local presets are single-config `Ninja` build trees. The configure preset name maps directly to its build directory, for example `windows-release -> build/windows-release`.

`*-release` presets currently default to `GLAB_BUILD_TESTS=OFF` and `GLAB_ENABLE_UNITY_BUILD=ON` for faster optimized builds. Use `*-release-with-tests` when you want a Release build that also compiles and runs the test targets.

`ci-*` presets are reserved for CI parity. They enable the same test coverage and release/debug profile split used by the GitHub Actions matrix.

### Package and Stage Runtime

```bash
# Example: Windows release packaging flow
cmake --preset windows-release
cmake --build --preset build-windows-release-package-content
cmake --build --preset build-windows-release-stage-runtime
cmake --install build/windows-release
```

`build-*-release-package-content` runs `rtr_asset_cook` and `rtr_asset_pack`, producing packaged runtime content under `build/<preset>/Stage/Release`.

`build-*-release-stage-runtime` additionally copies the `RTRLab` executable into the same staged runtime directory. `cmake --install` expects that staging target to have been built first.

### Build Manually

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/<preset>/bin/<Debug|RelWithDebInfo|Release>/RTRLab
```

Examples:

- `build/windows-debug/bin/Debug/RTRLab.exe`
- `build/windows-release/bin/Release/RTRLab.exe`
- `build/linux-debug/bin/Debug/RTRLab`
- `build/macos-release/bin/Release/RTRLab`

---

## Testing

The project uses **Google Test** (fetched automatically via CMake FetchContent).

```bash
# Debug test pass
cmake --preset windows-debug
cmake --build --preset build-windows-debug --parallel
ctest --preset test-windows-debug

# Release test pass
cmake --preset windows-release-with-tests
cmake --build --preset build-windows-release-with-tests --parallel
ctest --preset test-windows-release
```

Available test executables include `rtrlab_unit_tests`, `rtrlab_integration_tests`, `rtrlab_dev_profile_tests`, and `rtrlab_shipping_profile_tests`.

Preset families:

- `test-<platform>-debug`, `test-<platform>-relwithdebinfo`, `test-<platform>-release`
- `test-<platform>-*-unit`
- `test-<platform>-*-integration`
- `test-<platform>-debug-dev-profile`
- `test-<platform>-release-shipping-profile`
- `test-ci-<platform>-debug` and `test-ci-<platform>-release`

### CMake Options

| Option                        | Default | Description                                                 |
| ----------------------------- | ------- | ----------------------------------------------------------- |
| `GLAB_BUILD_TESTS`            | `ON`    | Build the test suite                                        |
| `GLAB_ENABLE_WARNINGS`        | `ON`    | Enable strict compiler warnings                             |
| `GLAB_ENABLE_ASAN`            | `OFF`   | Enable AddressSanitizer (non-MSVC)                          |
| `GLAB_ENABLE_PCH`             | `ON`    | Enable precompiled headers                                  |
| `GLAB_ENABLE_UNITY_BUILD`     | `OFF`   | Merge selected `.cpp` files into unity batches              |
| `GLAB_ENABLE_MSVC_MP`         | `ON`    | Enable MSVC multi-processor compilation (`/MP`)             |
| `GLAB_ENABLE_RELEASE_SYMBOLS` | `OFF`   | Emit MSVC debug symbols for `Release` builds                |
| `GLAB_BUILD_IMGUI_DEMO`       | `OFF`   | Compile `imgui_demo.cpp` into the Dear ImGui static library |

Preset defaults can override these project-level defaults. In particular, the shipped `*-release` and `ci-*-release` presets enable unity build, and the plain `*-release` presets disable tests unless you opt into `*-release-with-tests`.

---

## Dependencies

| Library                                                    | Purpose                    | Source                                                       |
| ---------------------------------------------------------- | -------------------------- | ------------------------------------------------------------ |
| [GLFW](https://github.com/glfw/glfw)                       | Windowing & input          | Git submodule (`Vendor/glfw`, `3.4.0`)                       |
| [GLM](https://github.com/g-truc/glm)                       | Linear algebra             | Git submodule (`Vendor/glm`, `1.0.3`)                        |
| [Glad](https://glad.dav1d.de/)                             | OpenGL 4.6 loader          | `Vendor/glad/`                                               |
| [STB Image](https://github.com/nothings/stb)               | Image loading              | `Vendor/stb/`                                                |
| [Dear ImGui](https://github.com/ocornut/imgui)             | Debug GUI                  | Git submodule (`Vendor/imgui`, `1.92.6`, `docking` branch)   |
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
