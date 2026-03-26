# Demo Series Plan

从最简单的窗口开始，逐步引入渲染概念。每个 demo 有明确的视觉目标，同时验证 OpenGL/Metal 两个后端的 RHI 一致性。

**现有基础设施**：DemoBase 框架、MeshFactory（cube/plane/sphere/quad）、Material 系统、SceneRenderer（3-pass）、Slang 跨后端 shader 编译、Camera + DebugCameraController、IRenderCommand 显式渲染命令模型。

---

## Phase 1 — 基础渲染管线（直接使用 IRenderCommand）

这一阶段的 demo **不使用 SceneRenderer**，直接操作 IRenderCommand + IShader + 顶点数据。目的是从最底层验证 RHI 的基本功能。

### 01_ClearScreen

- **画面**：纯色窗口，ImGui 可调清屏颜色
- **验证点**：窗口创建、BeginFrame/EndFrame、BeginRenderPass/EndRenderPass、back buffer RenderTarget、present/swap
- **新增**：无（已有设施足够）
- **Shader**：无

### 02_Triangle

- **画面**：屏幕中央一个彩色三角形（顶点色插值）
- **验证点**：IVertexBuffer、IVertexArray、BufferLayout、IShader 绑定、DrawArrays
- **新增 Shader**：`FlatColor.slang` — 接受顶点位置和颜色，直接输出插值颜色，无 MVP 变换（NDC 坐标直接输入）
- **注意**：不用 MeshFactory，手动构建顶点数据（NDC 坐标），这是最底层的验证

### 03_TexturedQuad

- **画面**：屏幕中央一个贴了纹理的四边形
- **验证点**：IIndexBuffer（indexed draw）、ITexture2D 创建和采样、CreateTexture2DFromFile、texture slot 绑定
- **新增 Shader**：`UnlitTextured.slang` — 接受位置 + UV，采样一张纹理输出
- **新增资源**：一张测试纹理（如 `container.jpg`）

### 04_TransformedCube

- **画面**：一个持续旋转的纹理立方体，透视投影
- **验证点**：MVP 矩阵 uniform 上传（SetMat4）、深度测试（PipelineState.DepthTestEnabled）、3D 几何体
- **修改 Shader**：`UnlitTextured.slang` 加入 `u_MVP` uniform
- **新增**：可复用 MeshFactory::CreateCube()，手动管理 MVP 矩阵

### 05_Camera

- **画面**：纹理立方体场景 + WASD 自由视角飞行
- **验证点**：Camera 类（view/projection 分离）、DebugCameraController、InputActionMap、窗口 resize 处理
- **新增**：多个立方体散落在场景中，验证 per-draw model matrix 更新

---

## Phase 2 — 光照与材质（引入光照 Shader）

这一阶段引入一个新的 `BasicLit.slang` shader（Phong 光照，无阴影），仍然直接使用 IRenderCommand，不依赖 SceneRenderer。

### 06_BasicLighting

- **画面**：白色方向光照射下的纯色立方体 + 小立方体表示光源位置
- **验证点**：法线数据传递、光照计算（ambient + diffuse + specular）、多个 uniform 上传
- **新增 Shader**：`BasicLit.slang` — Phong 光照，接受 `u_ViewProjection`、`u_Model`、`u_NormalMatrix`、光照参数、材质参数
- **ImGui**：可调光照方向、颜色、强度

### 07_Materials

- **画面**：5 个球体，不同材质属性（哑光红、高光蓝、金属金、塑料绿、镜面铬）
- **验证点**：Material 类的属性系统（SetVec3/SetFloat）、UploadToShader、per-object 材质切换
- **ImGui**：每个球体的 albedo、specularPower、ambientStrength 可独立编辑

### 08_LightingMaps

- **画面**：带 diffuse map + specular map 的立方体，木箱边框高光效果
- **验证点**：多纹理绑定（TextureSlot::Albedo + 新增 TextureSlot::Specular）、shader 中多采样器
- **修改**：`BasicLit.slang` 增加 `u_SpecularMap` 采样器、`u_UseAlbedoMap`/`u_UseSpecularMap` 开关
- **新增资源**：diffuse map + specular map 纹理

### 09_MultipleLights

- **画面**：1 个方向光 + 4 个彩色点光源 + 1 个聚光灯（手电筒效果，跟随摄像机）
- **验证点**：多光源数据打包和上传、shader 中光源数组/循环、Light.h 扩展（PointLight、SpotLight）
- **修改**：`BasicLit.slang` 扩展为支持方向光 + 点光源数组 + 聚光灯
- **ImGui**：每个光源的参数可调

---

## Uniform 绑定迁移节点

Phase 1-2 的 demo 使用 name-based uniform 上传（SetMat4、SetFloat、SetVec3 等），这是最直观的教学方式，也足以验证两端的基本 uniform 路径。

**但从 Phase 3 开始（09_MultipleLights 完成后），demo 应迁移到 block-based 绑定**，理由：

1. 09_MultipleLights 的光源数组会让散装 uniform 调用爆炸（每个光源 5+ 个 SetVec3/SetFloat），这是天然的迁移动机
2. IShader 已有 `SetUniformBlock(binding, data, size)` 接口，OpenGL 端映射到 UBO，Metal 端映射到 setVertexBytes/setFragmentBytes
3. Metal 后端的 uniform staging buffer 策略（FlushUniforms）本质上就是 block 上传，散装 SetFloat 在 Metal 上只是写入 CPU staging buffer 再整体 flush，不如直接走 block 路径清晰

**迁移方式**：

- 定义 `PerFrameData`（ViewProjection、CameraPosition、光源数据）和 `PerDrawData`（Model、NormalMatrix、材质参数）两个 struct
- Shader 中对应 `ConstantBuffer<PerFrameData>` 和 `ConstantBuffer<PerDrawData>`，binding index 固定（如 0 = PerFrame, 1 = PerDraw）
- Phase 3+ 的 demo 统一使用 `SetUniformBlock()` 上传，不再调用散装 SetMat4/SetFloat
- Phase 1-2 的 demo 保留散装调用作为教学起点，不回头改

这个迁移本身也是一个 RHI 验证点：两端的 uniform block 路径是否一致。

---

## Phase 3 — 渲染状态与帧缓冲

### 10_DepthTesting

- **画面**：多个不透明立方体前后交错放置 + 地板，可视化深度缓冲（灰度图）
- **验证点**：PipelineState.DepthTestEnabled/DepthWriteEnabled 开关效果、深度缓冲读取和可视化
- **ImGui**：开关深度测试，对比有/无深度测试的画面（关闭后出现错误遮挡）
- **注意**：场景中只有不透明物体，不涉及 blending，纯粹隔离深度测试行为

### 11_StencilOutline

- **画面**：立方体带明亮轮廓线（两遍绘制：正常 + 放大纯色）
- **验证点**：RenderPassDescriptor 的 stencil load/store action、PipelineState 扩展（需新增 stencil 相关字段）、多遍绘制
- **新增 Shader**：`SolidColor.slang` — 纯色输出，用于轮廓
- **RHI 扩展**：PipelineState 需增加 `StencilTestEnabled`、`StencilFunc`、`StencilOp` 等字段

### 12_FaceCulling

- **画面**：开放的立方体（去掉一面），演示正面/背面剔除效果
- **验证点**：PipelineState.CullFaceEnabled / CullFront 切换
- **ImGui**：三种模式切换（无剔除 / 背面剔除 / 正面剔除），实时看到区别

### 13_Blending

- **画面**：场景中有不透明物体和半透明彩色玻璃，按距离排序渲染
- **验证点**：PipelineState.BlendEnabled、透明物体排序、渲染顺序（先不透明后透明）
- **RHI 扩展**：PipelineState 可能需增加 BlendFunc 配置

### 14_Framebuffer

- **画面**：场景渲染到离屏 FBO，再以全屏四边形显示，附带反色/灰度后处理
- **验证点**：IFramebuffer 创建（FramebufferSpecification）、离屏 color + depth attachment、IRenderTarget from framebuffer、全屏 quad 绘制
- **新增 Shader**：`PostProcess.slang` — 全屏 quad 采样 + 可选反色/灰度/边缘检测 kernel
- **ImGui**：后处理效果切换

---

## Phase 4 — 进阶技术（逐步引入 SceneRenderer 或构建更复杂管线）

### 15_Cubemap

- **画面**：天空盒 + 反射球体
- **验证点**：Cubemap 纹理加载和采样（RHI 需扩展 cubemap 支持）、天空盒渲染（深度写入关闭、最后绘制）
- **RHI 扩展**：ITextureCube 接口或 ITexture2D 扩展为支持 cubemap
- **新增 Shader**：`Skybox.slang`、修改 `BasicLit.slang` 增加环境反射

### 16_ShadowMapping

- **画面**：方向光阴影（已有 ShadowMapping demo 的精简教学版）
- **验证点**：多 pass 渲染（depth pass + lighting pass）、shadow map FBO、光空间矩阵
- **复用**：已有的 `ShadowDepth.slang`、`ForwardLit.slang`、ShadowPass/ForwardPass
- **与 Showcase 的区别**：更简单的场景，专注于阴影原理教学，ImGui 显示 shadow map

### 17_HDRBloom

- **画面**：明亮光源 + 光晕效果，对比 LDR/HDR
- **验证点**：float 格式 render target（TextureFormat 需扩展 RGBA16F）、多 pass（scene → bright extract → blur → composite）、tone mapping
- **新增 Shader**：`BrightExtract.slang`、`GaussianBlur.slang`、`ToneMapping.slang`

### 18_DeferredShading

- **画面**：G-buffer 可视化（position/normal/albedo）+ 大量点光源
- **验证点**：多 color attachment 同时写入（G-buffer）、G-buffer pass + lighting pass 分离、大量光源性能对比
- **现有基础**：FramebufferSpecification 已支持多 color attachment（`FramebufferAttachmentSpecification` 接受 attachment 列表），GLFramebuffer / MetalFramebuffer 已有多 attachment 创建逻辑。本 demo 的工作重心不是扩展 RHI，而是验证并补齐现有多 attachment 路径在 G-buffer 场景下的完整性（draw buffer 指定、shader MRT 输出约定、两端一致性）
- **新增 Shader**：`GBufferPass.slang`（MRT 输出 position/normal/albedo）、`DeferredLighting.slang`（屏幕空间光照）

### 19_SSAO

- **画面**：环境光遮蔽效果，对比开关前后
- **验证点**：屏幕空间技术、noise texture、kernel 采样
- **新增 Shader**：`SSAO.slang`、`SSAOBlur.slang`

### 20_ModelLoading

- **画面**：加载外部模型（如 backpack），应用已有的光照管线
- **验证点**：模型加载库集成（assimp 或 tinyobjloader）、多 mesh + 多 material 的资产管线
- **新增**：模型加载模块、资产目录结构

### 21_PBR

- **画面**：金属/粗糙度球体矩阵（横轴 roughness 0→1，纵轴 metallic 0→1）
- **验证点**：Cook-Torrance BRDF、PBR 材质参数、线性空间渲染
- **新增 Shader**：`PBR.slang`（GGX + Schlick + Smith）

### 22_IBL

- **画面**：HDR 环境贴图照亮 PBR 物体
- **验证点**：equirectangular → cubemap 转换、irradiance convolution、prefiltered specular + BRDF LUT
- **新增 Shader**：`EquirectToCubemap.slang`、`IrradianceConvolution.slang`、`PrefilterSpecular.slang`、`BRDF_LUT.slang`

---

## Phase 5 — Showcase（组合性展示）

这些不是教学 demo，是综合展示，组合多个阶段的技术。

| Demo | 描述 |
|---|---|
| MaterialPlayground | **已有** — 多材质球体实时编辑 |
| ShadowMapping | **已有** — PCF 阴影 + 调试视图 |
| RenderStatePlayground | 交互式开关 depth/stencil/cull/blend，对比效果 |
| PBRShowcase | PBR + IBL + HDR + Bloom 的完整场景 |

---

## 新增 Shader 清单

| Shader | 引入阶段 | 说明 |
|---|---|---|
| `FlatColor.slang` | 02_Triangle | 顶点色直出，无变换 |
| `UnlitTextured.slang` | 03_TexturedQuad | 纹理采样，04 加入 MVP |
| `BasicLit.slang` | 06_BasicLighting | Phong 光照，08 加贴图，09 加多光源 |
| `SolidColor.slang` | 11_StencilOutline | MVP + 纯色输出 |
| `PostProcess.slang` | 14_Framebuffer | 全屏 quad + 后处理效果 |
| `Skybox.slang` | 15_Cubemap | Cubemap 天空盒采样 |
| `BrightExtract.slang` | 17_HDRBloom | 亮度提取 |
| `GaussianBlur.slang` | 17_HDRBloom | 高斯模糊（ping-pong） |
| `ToneMapping.slang` | 17_HDRBloom | Reinhard/ACES tone mapping |
| `GBufferPass.slang` | 18_DeferredShading | MRT 输出 G-buffer |
| `DeferredLighting.slang` | 18_DeferredShading | 屏幕空间光照 |
| `SSAO.slang` | 19_SSAO | 屏幕空间环境光遮蔽 |
| `PBR.slang` | 21_PBR | Cook-Torrance BRDF |

已有可复用：`ForwardLit.slang`、`ShadowDepth.slang`、`TexturePreview.slang`

---

## RHI 扩展清单

按 demo 进度需要逐步扩展的 RHI 能力：

| Demo | 扩展 |
|---|---|
| 11_StencilOutline | PipelineState 增加 stencil 配置 |
| 13_Blending | PipelineState 增加 blend func 配置 |
| 15_Cubemap | Cubemap 纹理支持 |
| 17_HDRBloom | TextureFormat::RGBA16F 浮点 render target |
| 18_DeferredShading | 验证多 attachment 路径完整性（draw buffer 指定、MRT shader 输出约定） |

---

## 目录结构

```
src/demos/
  tutorial/
    01_ClearScreen/
    02_Triangle/
    03_TexturedQuad/
    ...
  showcase/
    MaterialPlayground/    (已有，移入)
    ShadowMapping/         (已有，移入)

assets/shaders/
  tutorial/
    FlatColor.slang
    UnlitTextured.slang
    BasicLit.slang
    SolidColor.slang
    PostProcess.slang
    Skybox.slang
    ...
  ForwardLit.slang         (已有，保持)
  ShadowDepth.slang        (已有，保持)
  TexturePreview.slang     (已有，保持)
```

---

## 后端一致性验证策略

每个 demo 天然就是一个 backend parity test：同样的 demo 代码通过 RHI 在 OpenGL 和 Metal 上运行，画面应该一致。

重点验证节点：
- **02_Triangle**：最小绘制路径（buffer + shader + draw）
- **04_TransformedCube**：MVP uniform + depth state
- **08_LightingMaps**：多纹理绑定
- **11_StencilOutline**：stencil 状态
- **14_Framebuffer**：离屏渲染 + render target 切换
- **18_DeferredShading**：MRT 支持

如果某个 demo 在两个后端表现不一致，就说明该层级的 RHI 抽象存在问题，可以精确定位。

---

## 实施优先级

建议先完成 Phase 1（01-05），这 5 个 demo 覆盖了 RHI 最核心的路径，完成后就能确认两个后端的基本绘制管线是正确的。之后按顺序推进即可，每个 Phase 都是完整可交付的。
