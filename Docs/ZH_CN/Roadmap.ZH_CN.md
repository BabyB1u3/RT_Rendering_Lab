# 路线图

本文档概述实时渲染实验室的长期开发计划。

项目目前处于大规模架构重构阶段。渲染系统正围绕现代显式 RHI 和 Slang 着色器从零重新设计。以下各阶段反映新的开发轨迹。

[English](../EN/roadmap.md)

---

## Phase 1 — 核心引擎基础 ✅

目标：建立稳定的、与后端无关的引擎基础，供所有后续渲染工作依赖。

- [x] 应用运行时 — 主循环、`Layer`、`LayerStack`、运行时热切换
- [x] 事件系统 — `EventBus`、`ScopedConnection`、解耦的发布/订阅模型
- [x] 输入系统 — `InputAction` 绑定、键盘/鼠标码、输入回放
- [x] 资源系统 — 逻辑路径解析、多挂载点（Project / Engine / Plugins）、资源 catalog、cook 管线（`.rtrtex`、`.rtrpak`）、覆盖层支持
- [x] 诊断系统 — 结构化日志（spdlog）、断言框架、崩溃处理
- [x] 序列化 — 引擎侧序列化工具
- [x] 场景系统 — `Camera`、`DebugCameraController`、`Light`、`Transform`、`SceneData`
- [x] GUI — `ImGuiLayer`、调试面板、Demo 选择器；已接入 Metal 后端
- [x] 测试基础设施 — Google Test，单元测试与契约测试套件

---

## Phase 2 — RenderSystem：多后端 RHI 🔄

目标：设计并实现基于 Slang 着色器的现代显式 RHI，至少完成一个生产级后端。这是项目当前的全部重心。

设计文档：[Docs/Modules/RenderSystem/Design/](../Modules/RenderSystem/Design/RHI.md)

**着色器系统**
- [ ] Slang 集成 — 编译器接入、模块缓存、按后端编译
- [ ] 反射驱动的参数布局 — 基于 Slang 反射推导的中性布局模型
- [ ] 参数写入器 — 类型安全的 C++ → 着色器参数填写
- [ ] 着色器热重载 — 模块级重编译，无需重建完整 pipeline

**RHI 层**
- [ ] 核心资源类型 — `Buffer`、`Texture`、`Sampler`、`ShaderProgram`、`GraphicsPipeline`、`ComputePipeline`
- [ ] Pipeline 布局 — `PipelineLayout`、`ResourceSet`、反射驱动绑定
- [ ] 命令录制 — `CommandList`、绘制调用、资源屏障
- [ ] 交换链与帧呈现

**后端**
- [ ] Metal 后端 — 直接槽位绑定（v1），参数缓冲迁移路径（v2）
- [ ] Vulkan 后端 — 描述符集、渲染通道、同步

**第一个渲染 Demo**
- [ ] 新 RHI 上的前向渲染 Pass
- [ ] 基础方向光照
- [ ] ImGui 与新后端集成

---

## Phase 3 — 基础实时渲染

目标：在新 RHI 之上实现基础渲染技术。

- [ ] 阴影贴图 — 深度 Pass、PCF 软阴影、偏移策略
- [ ] 多光源类型 — 点光源、聚光灯
- [ ] 法线贴图
- [ ] 模型加载 — OBJ / glTF
- [ ] 天空盒 / 环境贴图
- [ ] 材质参数 UI — 逐 Demo 的 ImGui 控制面板

交付物：

- 光照与阴影 Demo
- 可导入的带贴图场景

---

## Phase 4 — 现代渲染管线

目标：过渡到更高级的渲染管线。

- [ ] 延迟渲染 — G-buffer 架构
- [ ] HDR 渲染与色调映射（Reinhard、ACES）
- [ ] Bloom — 阈值 + 高斯模糊 + 合成
- [ ] Gamma 校正管线
- [ ] 抗锯齿 — MSAA、FXAA

交付物：

- 延迟渲染 Demo
- 前向渲染与延迟渲染对比
- HDR / Bloom Demo

---

## Phase 5 — 基于物理的渲染

目标：实现基于物理的着色。

- [ ] Cook-Torrance BRDF — GGX / Schlick / Smith
- [ ] 金属度 / 粗糙度工作流
- [ ] 基于图像的光照（IBL）
- [ ] 环境贴图预过滤
- [ ] BRDF 积分查找表
- [ ] PBR 材质编辑器

交付物：

- PBR 材质球 Demo
- HDR 环境光照

---

## Phase 6 — 屏幕空间效果

目标：通过屏幕空间技术提升视觉真实感。

- [ ] 屏幕空间环境光遮蔽（SSAO）
- [ ] 屏幕空间反射（SSR）
- [ ] 运动模糊
- [ ] 景深
- [ ] 屏幕空间全局光照（实验性）

交付物：

- 带开关对比的视觉效果 Demo
- 参数实时调节工具

---

## Phase 7 — 程序化几何

目标：探索算法驱动的内容生成。

- [ ] 噪声地形 — Perlin、Simplex
- [ ] 带 LOD 的程序化地貌
- [ ] 体素地形实验
- [ ] 基于 Chunk 的世界表示
- [ ] Marching Cubes / Dual Contouring

交付物：

- 程序化地形 Demo
- 支持实时参数编辑的交互式可视化

---

## Phase 8 — GPU 驱动渲染

目标：利用现代 GPU 编程技术。

- [ ] 计算着色器
- [ ] GPU 粒子系统
- [ ] GPU 视锥体 / 遮挡剔除
- [ ] 间接渲染（MultiDrawIndirect）
- [ ] GPU 驱动管线

交付物：

- GPU 粒子模拟 Demo
- 性能对比实验

---

## Phase 9 — 光线追踪实验

目标：探索基于光线的渲染技术。

- [ ] CPU 光线追踪参考渲染器
- [ ] BVH 加速结构
- [ ] 带重要性采样的路径追踪
- [ ] 光栅化 + 光线追踪混合渲染

交付物：

- 基础路径追踪器
- 光线追踪可视化工具

---

## Phase 10 — 高级研究方向

未来探索方向，内容可能随时间演进：

- 体积渲染（雾、云、丁达尔光）
- 全局光照（RSM、LPV、体素锥追踪）
- 虚拟阴影贴图
- Mesh Shader
- 神经网络渲染实验
- 实时 / 离线混合渲染技术
