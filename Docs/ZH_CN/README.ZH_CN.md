# 实时渲染实验室

这是一个现代 C++ 图形引擎实验项目，主要用于探索实时渲染技术。长期目标是构建一个由 **Slang** 着色器和现代显式 RHI 驱动的多后端渲染器（Vulkan、Metal），并逐步补齐涵盖 PBR、阴影、屏幕空间效果等主题的渲染示例。

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
- 支持 **两种后端：Vulkan、Metal**。Vulkan 是主要的跨平台目标，Metal 负责 Apple 平台原生后端。
- 对外暴露 **Vulkan 风格 API**，显式建模 `ShaderProgram`、`PipelineLayout`、`ResourceSet`、`GraphicsPipeline`、`CommandList` 等对象。
- 参数绑定采用 **反射驱动**，由 Slang 反射结果作为布局真源，不再在 C++ 中手工维护绑定表。

等新渲染系统达到可用状态后，会逐步补充新的渲染演示。

---

## 项目结构

```text
RT_Rendering_Lab/
├─ .github/                 # GitHub Actions 工作流与 PR 元数据
├─ Archive/                 # 上一代渲染器遗留代码
├─ CMake/                   # 共享 CMake 模块、presets、测试辅助脚本
├─ Docs/
│  ├─ EN/
│  ├─ Modules/              # 设计、实现细节、计划与 code review
│  └─ ZH_CN/
├─ Engine/                  # 引擎侧源资源与配置内容
├─ Project/                 # 项目侧源资源与配置内容
├─ Src/
│  ├─ Core/
│  │  ├─ App/               # Application、Window、Layer、LayerStack
│  │  ├─ Diagnostics/       # Assert、Crash、Logging
│  │  ├─ Event/             # EventBus、Events、ScopedConnection
│  │  ├─ Input/             # Action、Device、Code、Replay
│  │  ├─ Resource/          # Catalog、Cook、IO、Mount、Package、Path
│  │  ├─ Serialization/     # PropertyTree、JSON backend、traits
│  │  └─ Util/              # Base、CommandLine、Time
│  ├─ Demos/
│  │  └─ 01_HelloWindow/
│  ├─ GUI/
│  │  ├─ Backends/
│  │  └─ Panels/
│  ├─ Scene/                # Camera、Controller、Transform、SceneData
│  ├─ Tools/                # `rtr_asset_cook`、`rtr_asset_pack`
│  ├─ main.cpp
│  └─ pch.h
├─ Tests/
│  ├─ Integration/
│  ├─ Smoke/
│  ├─ Support/
│  ├─ Unit/
│  ├─ CMakeLists.txt
│  └─ pch.h
├─ Tools/                   # 仓库级辅助脚本与工具
├─ Vendor/                  # 第三方依赖
├─ .rtrproject              # 项目标记文件
├─ CMakeLists.txt
├─ CMakePresets.json
└─ README.md
```

---

## 构建

### 环境要求

- 支持 C++20 的编译器（MSVC、GCC、Clang）
- CMake 3.20+
- Ninja
- 在 Windows 上，请从 Developer PowerShell / Developer Command Prompt 启动，以确保 MSVC 工具链在 `PATH` 中
- 在 Linux 上，请安装 GLFW/CI 所需依赖：`libgl-dev`、`libxrandr-dev`、`libxinerama-dev`、`libxcursor-dev`、`libxi-dev`、`libxkbcommon-dev`、`libwayland-dev`
- 在 macOS 上，请先执行 `brew install ninja`

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
- `Eigen`: `5.0.0`
- `Dear ImGui`: `1.92.6`（`docking` 分支）

### 使用 Presets 构建

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

# Windows - Release（包含测试）
cmake --preset windows-release-with-tests
cmake --build --preset build-windows-release-with-tests --parallel

# Linux - Debug
cmake --preset linux-debug
cmake --build --preset build-linux-debug --parallel

# Linux - Release
cmake --preset linux-release
cmake --build --preset build-linux-release --parallel

# Linux - Release（包含测试）
cmake --preset linux-release-with-tests
cmake --build --preset build-linux-release-with-tests --parallel

# macOS - Debug
cmake --preset macos-debug
cmake --build --preset build-macos-debug --parallel

# macOS - Release
cmake --preset macos-release
cmake --build --preset build-macos-release --parallel

# macOS - Release（包含测试）
cmake --preset macos-release-with-tests
cmake --build --preset build-macos-release-with-tests --parallel
```

当前仓库提供的本地 preset 都是单配置的 `Ninja` 构建树。configure preset 名称会直接映射到构建目录，例如 `windows-release -> build/windows-release`。

`*-release` preset 默认会设置 `GLAB_BUILD_TESTS=OFF`，并启用 `GLAB_ENABLE_UNITY_BUILD=ON`，用于更快的优化构建。如果你需要在 Release 下编译并运行测试，请使用 `*-release-with-tests`。

`ci-*` preset 主要用于与 GitHub Actions 保持一致，它们对应 CI 矩阵中的 debug / release 配置和测试范围。

### 打包与运行时 Stage

```bash
# 以 Windows Release 为例
cmake --preset windows-release
cmake --build --preset build-windows-release-package-content
cmake --build --preset build-windows-release-stage-runtime
cmake --install build/windows-release
```

`build-*-release-package-content` 会运行 `rtr_asset_cook` 和 `rtr_asset_pack`，将运行时内容输出到 `build/<preset>/Stage/Release`。

`build-*-release-stage-runtime` 会在同一目录下额外放入 `RTRLab` 可执行文件。执行 `cmake --install` 之前，需要先完成这个 stage 目标。

### 手动构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 运行

```bash
./build/<preset>/bin/<Debug|RelWithDebInfo|Release>/RTRLab
```

例如：

- `build/windows-debug/bin/Debug/RTRLab.exe`
- `build/windows-release/bin/Release/RTRLab.exe`
- `build/linux-debug/bin/Debug/RTRLab`
- `build/macos-release/bin/Release/RTRLab`

---

## 测试

项目使用 **Google Test**，通过 CMake `FetchContent` 自动获取。

```bash
# Debug 测试
cmake --preset windows-debug
cmake --build --preset build-windows-debug --parallel
ctest --preset test-windows-debug

# Release 测试
cmake --preset windows-release-with-tests
cmake --build --preset build-windows-release-with-tests --parallel
ctest --preset test-windows-release
```

测试可执行文件包括：`rtrlab_unit_tests`、`rtrlab_integration_tests`、`rtrlab_dev_profile_tests`、`rtrlab_shipping_profile_tests`。

常用测试 preset 分组：

- `test-<platform>-debug`、`test-<platform>-relwithdebinfo`、`test-<platform>-release`
- `test-<platform>-*-unit`
- `test-<platform>-*-integration`
- `test-<platform>-debug-dev-profile`
- `test-<platform>-release-shipping-profile`
- `test-ci-<platform>-debug` 与 `test-ci-<platform>-release`

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

注意：上表描述的是项目级默认值，具体 preset 还会覆盖它们。当前仓库自带的 `*-release` 和 `ci-*-release` preset 会启用 unity build，而普通 `*-release` preset 默认关闭测试；如需 Release 测试，请改用 `*-release-with-tests`。

---

## 依赖

| 库 | 作用 | 来源 |
| --- | --- | --- |
| [GLFW](https://github.com/glfw/glfw) | 窗口与输入 | Git 子模块（`Vendor/glfw`，`3.4.0`） |
| [Eigen](https://eigen.tuxfamily.org/) | 线性代数 | Git 子模块（`Vendor/eigen`，`5.0.0`） |
| [STB Image](https://github.com/nothings/stb) | 图像加载 | `Vendor/stb/` |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试 GUI | Git 子模块（`Vendor/imgui`，`1.92.6`，`docking` 分支） |
| [spdlog](https://github.com/gabime/spdlog) | 日志系统 | `Vendor/spdlog/` |
| [Google Test](https://github.com/google/googletest) | 测试框架 | CMake FetchContent |
| Slang | 着色语言与编译器 | 规划中 |

---

## 长期方向

当前的近期目标是完成新的 RenderSystem，包括 RHI 层、Slang 集成，以及 Vulkan / Metal 主线后端，使新的渲染演示能够重新跑起来。

之后项目还会继续探索：

- 基于物理的着色（Cook-Torrance、IBL）
- 现代实时渲染流程（Deferred、HDR、Tone Mapping）
- 屏幕空间技术（SSAO、SSR、Motion Blur）
- GPU Driven Rendering（Compute、Indirect Draw）
- 程序化生成（地形、体素）
- 光线追踪实验（Path Tracing、Hybrid Rendering）

完整开发计划请参考 [Docs/EN/roadmap.md](../EN/roadmap.md)。
