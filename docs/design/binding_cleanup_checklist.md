# Material / Shader Binding / Metal Backend 开工清单

这份小文档用于把当前设计结论收敛成一份可执行的开工清单。

相关设计文档：

- `docs/design/material_system.md`
- `docs/design/shader_binding.md`
- `docs/design/metal_backend.md`

---

## 1. 当前判断

项目下一阶段的主线不是先继续扩展新渲染特性，而是先把下面这条生产路径收尾到正式状态：

- 最终 multi-set 资源布局
- material / draw / frame-pass 的清晰资源归属
- 反射驱动的统一上传路径
- bridge / compatibility 层清理
- Metal 的测试与 CI 覆盖

当前代码仍然处于过渡态，典型信号包括：

- `Material` 仍依赖 `TextureSlot`
- `Material::UploadToShader()` 仍保留 legacy 名称上传路径
- `ForwardPass` 仍手工读取 material 属性并维持 flat `{0, N}` 绑定布局
- `ShaderBindingSets::FramePass/Material/Draw` 已存在，但尚未成为真实生产布局
- Metal backend 已可运行，但还没有进入完整自动化验证闭环

---

## 2. 为什么现在不优先做 Push Constants

`push constants` 目前放在靠后位置，不是因为它不重要，而是因为它还不是主线路径上的阻塞项。

原因如下：

1. 当前真正未完成的是资源组织和材质绑定模型，而不是小块常量传递接口。
2. `PerMaterial` / `PerDraw` / `PerPass` 的最终归属还没完全定型，现在过早定义 push-constant 风格 API，后面很容易返工。
3. 它是跨后端的公共 RHI 边界问题，应该在正常生产路径稳定后再决定是否真的需要。

因此当前策略是：

- 先完成最终资源模型和反射驱动上传
- 再判断哪些数据真的需要 push constants
- 最后明确 API 方向：引入，或显式延期

---

## 3. 优先级清单

### P0. 固化最终 multi-set 资源布局

目标：

- `set 0` = frame / pass
- `set 1` = material
- `set 2` = draw / instance

要做的事：

- 更新生产 shader 的逻辑 binding 布局
- 更新 pass 代码的 binding point
- 让 `ShaderBindingSets::FramePass`、`Material`、`Draw` 成为真实运行时布局，而不是预留常量

完成标志：

- 生产路径不再依赖 flat `{0, N}` 兼容布局

### P1. 重做 Material 资源模型

目标：

- 从 `TextureSlot + string property bag + UploadToShader()` 迁移到反射驱动的 material 资源所有权模型

要做的事：

- 弱化或移除 `TextureSlot` 作为公共 GPU 绑定契约
- 明确哪些字段属于 `PerMaterial`
- 明确哪些字段属于 `PerDraw`
- 让 material 持有与反射布局对应的 packed data / resource map

完成标志：

- pass 不再需要手工维护 material 的旧式贴图槽约定

### P2. 改造生产 Pass

重点先做：

- `ForwardPass`
- `ShadowPass`
- 必要时同步检查 `TexturePreviewPass`

要做的事：

- `ForwardPass` 不再直接从 material 手工提取 `u_Albedo`、`u_SpecularPower`、`u_AmbientStrength`
- frame / pass、material、draw 数据分别绑定
- 统一走反射驱动的 block packing

完成标志：

- 生产 pass 的上传路径与最终资源所有权模型一致

### P3. 清理过渡层和旧调用点

要做的事：

- 迁移仍依赖 deprecated flat overload 的代码
- 收窄 `MakeFlatShaderBindingPoint()` 的使用范围
- 迁移仍把 `SetUniformBlock(binding, data, size)` 当正常生产路径使用的 demo / tutorial / test

完成标志：

- maintained renderer 和 maintained demos 以 `ShaderBindingPoint` 为常规路径

### P4. 补全 metadata contract

要做的事：

- 明确 logical resource identity 的稳定 schema
- 明确 backend-local binding translation 的稳定 schema
- 明确 reflected block layout 的稳定 schema
- 让 fallback-to-flat 成为临时兼容行为，而不是默认契约

完成标志：

- OpenGL / Metal 在正常路径上依赖的是明确 metadata，而不是兜底猜测

### P5. 补 Metal 测试和 macOS CI

要做的事：

- 新增 Metal contract / integration tests
- 覆盖 shader/device 创建、binding resolution、reflection layout、格式和 readback 行为
- 增加 macOS CI job，自动构建并运行相关测试

完成标志：

- Metal 成为持续验证目标，而不是主要依赖本地手测

### P6. 最后处理 hardening 和可选项

包括：

- 减少不必要的同步等待
- 增强 Metal 运行时诊断
- 重新评估 `push constants`
- 评估离线 `.metallib` 打包

---

## 4. 建议的第一批开工范围

第一批不要铺太大，建议集中做一条闭环：

1. 先改 `ForwardLit` 及其相关 binding 布局，确定最终 `set 0 / 1 / 2` 分工。
2. 同步改 `Material` 的最小可用新接口，让 material 能表达 `PerMaterial` 数据和 material textures。
3. 改 `ForwardPass`，把 material 相关上传移出 pass-local 手工提取逻辑。
4. 用现有 OpenGL 测试先验证新布局，再准备追加 Metal 测试。

这样做的原因是：

- `ForwardPass` 是当前最典型的过渡态样本
- 它同时覆盖 shader binding、material ownership、reflected packing 三条主线
- 先把这一条跑通，后面再迁 `ShadowPass` 和其他路径，风险最低

---

## 5. 首批可能会改动的文件

- `src/graphics/Material.h`
- `src/graphics/Material.cpp`
- `src/renderer/passes/ForwardPass.cpp`
- `src/renderer/passes/ShadowPass.cpp`
- `src/graphics/ShaderBinding.h`
- `src/graphics/interfaces/IShader.h`
- `assets/shaders/` 下与 `ForwardLit` / `ShadowDepth` 相关的 shader
- `tests/` 下与 shader binding、render pass、backend contract 相关的测试

---

## 6. 开工原则

1. 先让最终生产路径成立，再删除兼容层。
2. 先让 OpenGL 和 Metal 共享同一套逻辑模型，再做后端特化优化。
3. 先解决 ownership 和 layout 问题，再讨论 push constants 这类 API 精修项。
4. 每一轮改动都要让测试面更接近最终架构，而不是继续加深过渡态依赖。
