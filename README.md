# Real Time Rendering Lab

A modern C++ / OpenGL graphics playground for experimenting with real-time rendering techniques and graphics algorithms.

This repository is designed as a long-term platform for graphics experiments, learning, and visualization.
It provides a lightweight framework where different rendering techniques can be implemented and explored as independent demos.

[中文文档](./docs/zh-CN/README.zh-CN.md)

---

## Features

- **Demo Framework** — modular architecture where each rendering technique lives as an independent, hot-switchable demo
- **Forward Rendering Pipeline** — multi-pass renderer with shadow mapping support
- **Shadow Mapping** — directional light depth pass with bias-aware shadow comparison and debug visualization
- **Material System** — shader + texture slot binding with per-object materials
- **Procedural Meshes** — built-in cube, plane, and fullscreen quad generators
- **ImGui Integration** — debug panels for framerate, memory, and demo selection
- **First-Person Camera** — WASD + mouse look with scroll-wheel FOV control
- **Framebuffer Abstraction** — off-screen rendering with multiple color/depth attachments and pixel read-back
- **DSA OpenGL** — modern Direct State Access style for shaders, buffers, textures, and framebuffers

---

## Demo Gallery

| Demo | Description |
|------|-------------|
| **Shadow Mapping** | Directional light shadow mapping with depth pass, forward lit pass, and shadow map debug preview |

Screenshots and visual results will be added as the demos evolve.

---

## Project Structure

```
RT_Rendering_Lab/
├── src/
│   ├── core/           # Application, Window, Input, Time, Logger, FileSystem
│   ├── graphics/       # Shader, Texture, Buffers, Mesh, Material, Framebuffer, RenderCommand
│   ├── renderer/       # SceneRenderer, ForwardPass, ShadowPass, TexturePreviewPass
│   ├── scene/          # Camera, DebugCameraController, Light, Transform, SceneData
│   ├── demos/          # DemoBase, DemoRegistry, LabLayer, ShadowMapping/
│   ├── gui/            # ImGuiLayer, DebugPanel, DemoSelectorPanel
│   └── main.cpp
├── assets/
│   ├── shaders/        # GLSL shaders (ForwardLit, ShadowDepth, TexturePreview)
│   ├── models/
│   ├── textures/
│   └── scenes/
├── tests/
│   ├── unit/           # Time, LayerStack, Transform, Camera, Buffers, CameraController
│   ├── integration/    # Shader, Texture, Framebuffer (require OpenGL context)
│   └── support/        # GLTestContext, MathTestUtils, TestLayer
├── docs/
│   └── roadmap.md
├── vendor/             # Third-party: GLFW, GLM (submodules), Glad, STB, ImGui
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

### Build with Presets

The project ships with CMake presets for common configurations:

```bash
# Visual Studio 2022 — Debug
cmake --preset windows-vs-debug
cmake --build build/windows-vs-debug

# Ninja — Release
cmake --preset ninja-release
cmake --build build/ninja-release
```

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

Test executables: `rtrlab_unit_tests`, `rtrlab_integration_tests`.

Integration tests create a hidden OpenGL context — they require a GPU or software renderer.

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `GLAB_BUILD_TESTS` | `ON` | Build the test suite |
| `GLAB_ENABLE_WARNINGS` | `ON` | Enable strict compiler warnings |
| `GLAB_ENABLE_ASAN` | `OFF` | Enable AddressSanitizer (non-MSVC) |

---

## Dependencies

| Library | Purpose | Source |
|---------|---------|--------|
| [GLFW](https://github.com/glfw/glfw) | Windowing & input | Git submodule |
| [GLM](https://github.com/g-truc/glm) | Linear algebra | Git submodule |
| [Glad](https://glad.dav1d.de/) | OpenGL 4.6 loader | `vendor/glad/` |
| [STB Image](https://github.com/nothings/stb) | Image loading | `vendor/stb/` |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug GUI | `vendor/imgui/` |
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
