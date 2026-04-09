# 实时渲染实验室

一个基于现代 C++ 的图形引擎，用于探索实时渲染技术。长期目标是构建一个由 **Slang** 着色器驱动的多后端渲染器（Vulkan、Metal、OpenGL），配合现代显式 RHI，并实现一系列涵盖 PBR、阴影、屏幕空间效果等技术的渲染 Demo。

[English](../../README.md)

---

## 当前状态

项目正处于**大规模架构重构阶段**。渲染系统正在从零重新设计，上一版的渲染相关代码已整体移入 `Archive/`。核心引擎系统持续活跃开发中。

| 模块 | 状态 |
|------|------|
| Core — App、Layer、Event | 活跃 |
| Core — 输入系统 | 活跃 |
| Core — 资源系统 | 活跃 |
| Core — 诊断、序列化 | 活跃 |
| Scene 系统 | 活跃 |
| GUI / ImGui 集成 | 活跃 |
| **RenderSystem** | **离线 — 全面重新设计中** |

---

## 架构方向

新渲染系统围绕现代显式 RHI 设计。设计文档位于 [Docs/Modules/RenderSystem/Design/](../Modules/RenderSystem/Design/RHI.md)。

核心决策：

- **Slang** 着色器语言 — 单一源码按后端分别编译，替代旧的 GLSL → SPIR-V 管线
- **三大后端：Vulkan · Metal · OpenGL** — Vulkan 和 Metal 为主要质量目标；OpenGL 为兼容性后端
- **Vulkan 风格公共 API** — 显式对象（`ShaderProgram`、`PipelineLayout`、`ResourceSet`、`GraphicsPipeline`、`CommandList`）；无隐式全局状态
- **反射驱动的参数绑定** — Slang 反射是所有着色器参数布局的唯一来源；C++ 侧无手工维护的绑定表

渲染 Demo 将在新系统达到可用状态后陆续加入。

---

## 项目结构

```
RT_Rendering_Lab/
├── Src/
│   ├── Core/
│   │   ├── App/            # Application、Window、Layer、LayerStack
│   │   ├── Event/          # EventBus、Events、ScopedConnection
│   │   ├── Input/          # InputAction、KeyCode、MouseCode、回放
│   │   ├── Resource/       # 逻辑路径、catalog、cook、pack、mount、IO
│   │   ├── Diagnostics/    # 日志、断言、崩溃处理
│   │   └── Serialization/
│   ├── GUI/
│   │   ├── Backends/Metal/
│   │   └── Panels/
│   ├── Scene/              # Camera、Light、Transform、SceneData
│   ├── Demos/
│   │   └── 01_HelloWindow/
│   ├── Tools/              # 资源索引、cooking、packaging 工具
│   └── main.cpp
│
├── Archive/                # 上一版渲染器退役代码
│   ├── graphics/           # 旧 IShader / ITexture2D / IFramebuffer + OpenGL 后端
│   ├── renderer/           # 旧 SceneRenderer、ForwardPass、ShadowPass
│   └── shaders/            # 旧 GLSL 源码 + SPIR-V
│
├── Docs/
│   ├── Modules/
│   │   ├── RenderSystem/Design/   # RHI.md、ShaderSystem.md、各后端文档
│   │   ├── ResourceSystem/
│   │   ├── DiagnosticsSystem/
│   │   ├── SerializationSystem/
│   │   └── InputEventSystem/
│   ├── EN/
│   └── ZH_CN/
│
├── Content/                # 运行时资源（模型、纹理、场景）
├── Tests/
│   ├── Unit/
│   ├── Contract/
│   └── Support/
├── Vendor/                 # GLFW、GLM、ImGui、glslang、SPIRV-Cross、Glad、STB、spdlog
└── CMakeLists.txt
```

---

## 构建

### 环境要求

- 支持 C++20 的编译器（MSVC、GCC、Clang）
- CMake 3.20+
- 支持 OpenGL 4.6 的 GPU 及驱动（用于 OpenGL 相关测试）

其余依赖已包含在 `Vendor/` 目录中或由 CMake 自动获取。

### 克隆仓库

```bash
git clone --recursive <repository-url>
cd RT_Rendering_Lab
```

> 如果克隆时未使用 `--recursive`，请执行 `git submodule update --init --recursive`。

当前固定的子模块版本：

- `GLFW`：`3.4.0`
- `GLM`：`1.0.3`
- `Dear ImGui`：`1.92.6`（`docking` 分支）
- `glslang`：`vulkan-sdk-1.4.304.1`
- `SPIRV-Cross`：`vulkan-sdk-1.4.304.1`

### 使用预设构建

```bash
# Visual Studio 2026 — Debug
cmake --preset windows-vs-debug
cmake --build build/windows-vs-debug

# Visual Studio 2026 — 快速调试迭代
cmake --preset windows-vs-debug-fast
cmake --build --preset build-windows-debug-fast --parallel

# Ninja — Release
cmake --preset ninja-release
cmake --build build/ninja-release

# Ninja — 快速调试迭代
cmake --preset ninja-debug-fast
cmake --build --preset build-ninja-debug-fast
```

`windows-vs-debug-fast` 和 `ninja-debug-fast` 会关闭测试构建、开启 unity build 和预编译头、跳过 `imgui_demo.cpp`，用于缩短日常开发的编译循环。

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

项目使用 **Google Test**（通过 CMake FetchContent 自动获取）。

```bash
cmake --preset windows-vs-debug
cmake --build build/windows-vs-debug
ctest --test-dir build/windows-vs-debug
```

测试可执行文件：`rtrlab_unit_tests`、`rtrlab_contract_tests`。

### CMake 选项

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `GLAB_COMPILE_SHADERS` | `ON` | 构建时将 GLSL 着色器编译为 SPIR-V |
| `GLAB_BUILD_TESTS` | `ON` | 构建测试套件 |
| `GLAB_ENABLE_WARNINGS` | `ON` | 启用严格的编译器警告 |
| `GLAB_ENABLE_ASAN` | `OFF` | 启用 AddressSanitizer（非 MSVC） |
| `GLAB_ENABLE_PCH` | `ON` | 启用预编译头 |
| `GLAB_ENABLE_UNITY_BUILD` | `OFF` | 将部分 `.cpp` 合并为 unity 编译单元 |
| `GLAB_ENABLE_MSVC_MP` | `ON` | 为 MSVC 启用多进程编译（`/MP`） |
| `GLAB_BUILD_IMGUI_DEMO` | `OFF` | 是否将 `imgui_demo.cpp` 编译进 Dear ImGui 静态库 |

---

## 依赖库

| 库 | 用途 | 来源 |
|----|------|------|
| [GLFW](https://github.com/glfw/glfw) | 窗口管理与输入处理 | Git 子模块（`Vendor/glfw`，`3.4.0`） |
| [GLM](https://github.com/g-truc/glm) | 线性代数运算 | Git 子模块（`Vendor/glm`，`1.0.3`） |
| [Glad](https://glad.dav1d.de/) | OpenGL 4.6 函数加载器 | `Vendor/glad/` |
| [STB Image](https://github.com/nothings/stb) | 图像文件加载 | `Vendor/stb/` |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试 GUI | Git 子模块（`Vendor/imgui`，`1.92.6`，`docking` 分支） |
| [glslang](https://github.com/KhronosGroup/glslang) | GLSL → SPIR-V 编译器 | Git 子模块（`Vendor/glslang`，`vulkan-sdk-1.4.304.1`） |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | SPIR-V → GLSL 转译器 | Git 子模块（`Vendor/spirv-cross`，`vulkan-sdk-1.4.304.1`） |
| [spdlog](https://github.com/gabime/spdlog) | 日志系统 | `Vendor/spdlog/` |
| [Google Test](https://github.com/google/googletest) | 测试框架 | CMake FetchContent |
| Slang | 着色器语言与编译器 | 计划中 |

---

## 长期方向

当前首要任务是完成新 RenderSystem：RHI 层、Slang 集成，以及至少一个后端（Metal 或 Vulkan）达到可运行渲染 Demo 的状态。

此后，项目将逐步探索：

- 基于物理的着色（Cook-Torrance、IBL）
- 现代实时渲染（延迟渲染、HDR、色调映射）
- 屏幕空间技术（SSAO、SSR、运动模糊）
- GPU 驱动渲染（计算着色器、间接绘制）
- 程序化生成（地形、体素）
- 光线追踪实验（路径追踪、混合渲染）

详细开发计划请参阅 [roadmap.zh-CN.md](./roadmap.zh-CN.md)。
