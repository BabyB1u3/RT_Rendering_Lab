# 材质实验场 Demo

一个交互式 Blinn-Phong 材质对比 Demo —— 五个 UV 球体分别使用不同的表面预设，
所有参数均可通过 ImGui 实时调节。

[English](../../en/demos/material-playground.md)

![材质实验场](../../screenshots/MaterialPlayground.png)

---

## 概述

本 Demo 在带光照的地面上放置五个球体，每个球体初始化为不同的 Blinn-Phong 材质预设
（哑光、高光、金属、塑料、铬合金）。所有材质参数 —— 漫反射色、高光度、环境光强度
—— 均可通过 ImGui 实时调节，便于直观对比各参数对最终着色的影响。

场景复用与阴影贴图 Demo 相同的多 Pass 前向渲染管线：
光源深度 Pass、带 PCF 阴影的前向着色 Pass，以及可选的调试可视化。

### 渲染管线

```
SceneRenderer::Render(scene)
├── ShadowPass::Execute()          → 2048x2048 深度图（正面剔除）
├── ForwardPass::Execute()         → Blinn-Phong 着色 + PCF 阴影采样
└── TexturePreviewPass::Execute()  → 最终输出或阴影贴图预览
```

### 光照模型

**Blinn-Phong**，支持逐球参数：

| 参数 | Uniform | 范围 | 效果 |
|------|---------|------|------|
| 漫反射色 | `u_Albedo` | RGB 颜色 | 表面基色 |
| 高光度 | `u_SpecularPower` | 1 – 512 | 高光锐度（越高越集中） |
| 环境光强度 | `u_AmbientStrength` | 0.0 – 1.0 | 阴影区域的基础照明 |

---

## 材质预设

| # | 名称 | 漫反射色 | 高光度 | 环境光 |
|---|------|----------|--------|--------|
| 1 | 哑光红 (Matte Red) | (0.9, 0.2, 0.15) | 4 | 0.20 |
| 2 | 亮蓝 (Shiny Blue) | (0.2, 0.3, 0.9) | 128 | 0.10 |
| 3 | 黄金 (Gold) | (0.83, 0.69, 0.22) | 64 | 0.12 |
| 4 | 塑料绿 (Plastic Green) | (0.1, 0.8, 0.2) | 32 | 0.15 |
| 5 | 铬合金 (Chrome) | (0.9, 0.9, 0.9) | 256 | 0.05 |

---

## 操作说明

| 输入 | 动作 |
|------|------|
| W / A / S / D | 前 / 左 / 后 / 右移动相机 |
| Q / E | 下降 / 上升相机 |
| 鼠标 | 旋转视角（始终启用） |
| 滚轮 | 调整视场角（FOV） |
| 1 | 显示最终渲染输出 |
| 2 | 显示阴影贴图调试视图 |

### ImGui 控件

**Material Playground** 面板提供以下控件：

| 控件 | 说明 |
|------|------|
| 输出模式 | 切换最终渲染输出和阴影贴图调试视图 |
| 光照方向 | 调整方向光方向（XYZ） |
| 光照颜色 | 修改光源颜色 |
| 光照强度 | 缩放光源亮度（0 – 5） |
| 逐球漫反射色 | 每个球体的基色拾色器 |
| 逐球高光度 | 高光锐度滑块（1 – 512） |
| 逐球环境光 | 环境光强度滑块（0 – 1） |

---

## 场景设置

- **地面**：15x15 平面，位于 Y=0，灰色材质（漫反射 0.6，高光 16）
- **5 个 UV 球体**：半径 1.5，沿 X 轴均匀排列于 Y=1，由 `MeshFactory::CreateSphere()` 生成（16 层 × 32 切片）
- **方向光**：方向 `(-0.5, -1.0, -0.3)`，白色，强度 1.2
- **相机**：初始位置 `(0, 3, 10)`，朝向略偏下方

---

## 文件结构

```
src/demos/MaterialPlayground/
├── MaterialPlayground.h           # Demo 类声明
└── MaterialPlayground.cpp         # 场景构建、材质预设、ImGui 面板、渲染调度

src/renderer/
├── SceneRenderer.h/cpp            # 多 Pass 渲染调度
└── passes/
    ├── ShadowPass.h/cpp           # 从光源视角的纯深度 Pass
    ├── ForwardPass.h/cpp          # 前向着色 + 阴影采样
    └── TexturePreviewPass.h/cpp   # 全屏纹理可视化

assets/shaders/
├── ForwardLit.vert / .frag        # Blinn-Phong + PCF 阴影采样（u_Albedo、u_SpecularPower、u_AmbientStrength）
├── ShadowDepth.vert / .frag       # 仅写入深度的着色器
├── TexturePreview.vert / .frag    # 全屏四边形可视化
└── *.spv                          # 编译后的 SPIR-V（构建时由 glslang 生成）
```

---

## 关键实现细节

| 方面 | 细节 |
|------|------|
| 球体网格 | UV 球体，16 层 × 32 切片，由 `MeshFactory::CreateSphere()` 生成 |
| 漫反射纹理 | 1x1 白色像素 — 颜色来自 `u_Albedo` uniform |
| 材质同步 | `SyncMaterialProperties()` 每帧将 ImGui 编辑的值推送到 `Material` |
| 阴影贴图 | 共享 `SceneRenderer` 管线 — 与阴影贴图 Demo 相同的 PCF / 偏移 / 正面剔除 |
| 球体布局 | 5 个球体位于 X = {-5, -2.5, 0, 2.5, 5}，Y = 1，Z = 0 |
