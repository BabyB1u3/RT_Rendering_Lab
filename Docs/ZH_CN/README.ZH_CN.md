# 实时渲染实验室

这是一个现代 C++ 图形引擎实验项目，主要用于探索实时渲染技术。长期目标是构建一个由 **Slang** 着色器和现代显式 RHI 驱动的多后端渲染器（Vulkan、Metal、OpenGL），并逐步补齐涵盖 PBR、阴影、屏幕空间效果等主题的渲染示例。

[English](../../README.md)

---

## 当前状态

项目目前正处于一次**较大的架构重构**中。渲染系统正在从头设计，上一代与渲染强相关的代码已经移动到 `Archive/`。核心引擎系统仍在持续开发。

| 模块 | 状态 |
| --- | --- |
| Core - App / Layer / Event | 开发中 |
| Core - Input System | 开发中 |
| Core - Resource System | 开发中 |
| Core - Diagnostics / Serialization | 开发中 |
| Scene System | 开发中 |
| GUI / ImGui Integration | 开发中 |
| **RenderSystem** | **离线重构中** |

---

## 架构方向

新的渲染系统围绕现代显式 RHI 设计。相关设计文档位于 [Docs/Modules/RenderSystem/Design/RHI.md](../Modules/RenderSystem/Design/RHI.md)。

关键设计决策：

- 使用 **Slang** 作为着色语言，用单一源码面向多后端编译，替代旧的 GLSL/SPIR-V 流程。
- 支持 **三种后端：Vulkan、Metal、OpenGL**。Vulkan 和 Metal 是主要目标，OpenGL 作为兼容后端保留。
- 对外暴露 **Vulkan 风格 API**，显式建模 `ShaderProgram`、`PipelineLayout`、`ResourceSet`、`GraphicsPipeline`、`CommandList` 等对象。
- 参数绑定采用 **反射驱动**，由 Slang 反射结果作为布局真源，不再在 C++ 中手工维护绑定表。

等新渲染系统达到可用状态后，会逐步补充新的渲染演示。

---

## 项目结构

```text
RT_Rendering_Lab/
├─ Src/
│  ├─ Core/
│  │  ├─ App/            # Application、Window、Layer、LayerStack
│  │  ├─ Event/          # EventBus、Events、ScopedConnection
│  │  ├─ Input/          # InputAction、KeyCode、MouseCode、Replay
│  │  ├─ Resource/       # Logical Path、Catalog、Cook、Pack、Mount、IO
│  │  ├─ Diagnostics/    # Logging、Assert、Crash
│  │  └─ Serialization/
│  ├─ GUI/
│  │  ├─ Backends/Metal/
│  │  └─ Panels/
│  ├─ Scene/             # Camera、Light、Transform、SceneData
│  ├─ Demos/
│  │  └─ 01_HelloWindow/
│  ├─ Tools/             # Asset indexing、cooking、packaging
│  └─ main.cpp
├─ Archive/              # 上一代渲染器遗留代码
├─ Docs/
├─ Content/              # 运行时资源
├─ Tests/
│  ├─ Unit/
│  ├─ Integration/
│  └─ Support/
├─ Vendor/               # GLFW、GLM、ImGui、Glad、STB、spdlog
└─ CMakeLists.txt
```

---

## 构建

### 环境要求

- 支持 C++20 的编译器（MSVC、GCC、Clang）
- CMake 3.20+
- 支持 OpenGL 4.6 的 GPU 与驱动（OpenGL 相关测试会依赖）

其他依赖要么已经包含在 `Vendor/` 中，要么由 CMake 自动获取。

### 克隆仓库

```bash
git clone --recursive <repository-url>
cd RT_Rendering_Lab
```

如果最初克隆时没有带 `--recursive`，请执行：

```bash
git submodule update --init --recursive
```

当前固定的子模块版本：

- `GLFW`: `3.4.0`
- `GLM`: `1.0.3`
- `Dear ImGui`: `1.92.6`（`docking` 分支）

### 使用 Presets 构建

```bash
# Visual Studio 2026 - Debug
cmake --preset windows-vs
cmake --build --preset build-windows-vs-debug

# Visual Studio 2026 - RelWithDebInfo
cmake --preset windows-vs
cmake --build --preset build-windows-vs-relwithdebinfo

# Visual Studio 2026 - Fast debug iteration without tests
cmake --preset windows-vs-fast
cmake --build --preset build-windows-vs-fast-debug --parallel

# Ninja - Release
cmake --preset linux-ninja-release
cmake --build --preset build-linux-ninja-release

# Ninja - RelWithDebInfo
cmake --preset linux-ninja-relwithdebinfo
cmake --build --preset build-linux-ninja-relwithdebinfo

# Ninja - Fast debug iteration
cmake --preset linux-ninja-debug-fast
cmake --build --preset build-linux-ninja-debug-fast

# macOS Ninja - Release
cmake --preset macos-ninja-release
cmake --build --preset build-macos-ninja-release

# macOS Ninja - RelWithDebInfo
cmake --preset macos-ninja-relwithdebinfo
cmake --build --preset build-macos-ninja-relwithdebinfo

# macOS Ninja - Fast debug iteration
cmake --preset macos-ninja-debug-fast
cmake --build --preset build-macos-ninja-debug-fast
```

在 Windows 上，`windows-vs` 是一棵共享的 multi-config 工程树；真正使用 `Debug`、`RelWithDebInfo` 还是 `Release`，由对应的 `build preset` 或 `test preset` 决定。

`windows-vs-fast`、`linux-ninja-debug-fast` 和 `macos-ninja-debug-fast` 会关闭测试并启用 unity build，用于缩短日常 edit-build-run 循环。

### 手动构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 运行

```bash
./build/<config>/RTRLab
```

---

## 测试

项目使用 **Google Test**，通过 CMake `FetchContent` 自动获取。

```bash
cmake --preset windows-vs
cmake --build --preset build-windows-vs-debug
ctest --preset test-windows-vs-debug
```

测试可执行文件包括：`rtrlab_unit_tests`、`rtrlab_integration_tests`。

### CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `GLAB_BUILD_TESTS` | `ON` | 构建测试套件 |
| `GLAB_ENABLE_WARNINGS` | `ON` | 启用严格编译警告 |
| `GLAB_ENABLE_ASAN` | `OFF` | 启用 AddressSanitizer（非 MSVC） |
| `GLAB_ENABLE_PCH` | `ON` | 启用预编译头 |
| `GLAB_ENABLE_UNITY_BUILD` | `OFF` | 将部分 `.cpp` 合并为 unity 编译批次 |
| `GLAB_ENABLE_MSVC_MP` | `ON` | 在 MSVC 下启用多进程编译（`/MP`） |
| `GLAB_ENABLE_RELEASE_SYMBOLS` | `OFF` | 为 MSVC `Release` 构建输出调试符号 |
| `GLAB_BUILD_IMGUI_DEMO` | `OFF` | 是否将 `imgui_demo.cpp` 编译进 Dear ImGui 静态库 |

---

## 依赖

| 库 | 作用 | 来源 |
| --- | --- | --- |
| [GLFW](https://github.com/glfw/glfw) | 窗口与输入 | Git 子模块（`Vendor/glfw`，`3.4.0`） |
| [GLM](https://github.com/g-truc/glm) | 线性代数 | Git 子模块（`Vendor/glm`，`1.0.3`） |
| [Glad](https://glad.dav1d.de/) | OpenGL 4.6 加载器 | `Vendor/glad/` |
| [STB Image](https://github.com/nothings/stb) | 图像加载 | `Vendor/stb/` |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试 GUI | Git 子模块（`Vendor/imgui`，`1.92.6`，`docking` 分支） |
| [spdlog](https://github.com/gabime/spdlog) | 日志系统 | `Vendor/spdlog/` |
| [Google Test](https://github.com/google/googletest) | 测试框架 | CMake FetchContent |
| Slang | 着色语言与编译器 | 规划中 |

---

## 长期方向

当前的近期目标是完成新的 RenderSystem，包括 RHI 层、Slang 集成，以及至少一个可运行的后端（Metal 或 Vulkan），使新的渲染演示能够重新跑起来。

之后项目还会继续探索：

- 基于物理的着色（Cook-Torrance、IBL）
- 现代实时渲染流程（Deferred、HDR、Tone Mapping）
- 屏幕空间技术（SSAO、SSR、Motion Blur）
- GPU Driven Rendering（Compute、Indirect Draw）
- 程序化生成（地形、体素）
- 光线追踪实验（Path Tracing、Hybrid Rendering）

完整开发计划请参考 [Docs/EN/roadmap.md](../EN/roadmap.md)。
