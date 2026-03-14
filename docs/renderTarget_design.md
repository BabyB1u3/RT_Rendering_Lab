# RenderTarget 设计方案（适用于 RT Rendering Lab）

## 1. 设计目标

`RenderTarget` 的目标不是替代 `Framebuffer`，而是在 **Renderer 层** 为“某个 pass 的输出目标”提供一个更高层、更有语义的抽象。

一句话概括：

- `Framebuffer`：OpenGL 资源对象，关注 **怎么存**
- `RenderTarget`：渲染流程对象，关注 **输出到哪里**

在当前项目中，`RenderTarget` 主要解决以下问题：

1. 统一 backbuffer 和 offscreen framebuffer
2. 让 pass 少依赖 OpenGL 细节
3. 为多 pass 渲染链提供统一的输出目标表示
4. 为后续 post-process / bloom / deferred / SSAO 等扩展留接口
5. 把“输出目标”从“绘制内容”里分离出来

---

## 2. RenderTarget 与现有模块的关系

### 2.1 与 RenderItem 的区别

`RenderItem` 表示：

- mesh
- material
- transform

也就是：

**画什么**

`RenderTarget` 表示：

- 当前 pass 的输出目标
- 输出尺寸
- 输出 color/depth attachments
- 是 backbuffer 还是 framebuffer

也就是：

**画到哪里**

所以两者不是一个层次：

- `RenderItem` = 输入内容
- `RenderTarget` = 输出容器

---

### 2.2 与 Framebuffer 的区别

`Framebuffer` 是底层实现对象：

- FBO id
- color/depth attachments
- bind/unbind
- resize
- completeness check

`RenderTarget` 是渲染流程对象：

- 当前 pass 是否输出到屏幕
- 是否输出到一个离屏 FBO
- 输出结果后续怎么被别的 pass 读取
- 是否有 color/depth 附件可供访问

所以：

**Framebuffer 是实现细节，RenderTarget 是 renderer 语义对象。**

---

### 2.3 与 RenderPass 的关系

`RenderPass` 关注：

- 如何执行一类绘制逻辑

`RenderTarget` 关注：

- 该 pass 的结果写到哪里

典型调用应当趋向于：

    shadowPass.Execute(scene, shadowTarget);
    forwardPass.Execute(scene, sceneColorTarget);
    previewPass.Execute(sceneColorTarget, backBufferTarget);

---

## 3. 当前项目阶段对 RenderTarget 的要求

当前项目不需要一个过度复杂的 `RenderTarget`。  
第一阶段的 RenderTarget 应该满足：

1. 能表示 **默认 backbuffer**
2. 能表示 **一个已有的 framebuffer**
3. 能统一提供：
   - `Bind()`
   - `Unbind()`
   - `GetWidth()/GetHeight()`
   - `GetColorAttachment()`
   - `GetDepthAttachment()`
4. 不负责资源创建策略，不替代 `Framebuffer`
5. 不负责 render graph，不做依赖分析
6. 不负责 scene / pass 调度

换句话说：

**第一阶段的 `RenderTarget` 应该是一个薄包装层（thin wrapper）。**

---

## 4. 第一阶段推荐设计：薄包装 RenderTarget

### 4.1 核心思想

RenderTarget 只表示两种类型：

- `BackBuffer`
- `FramebufferTarget`

内部只保存：

- 类型
- 如果是 framebuffer target，则保存 `Ref<Framebuffer>`
- 如果是 backbuffer，则保存宽高

这意味着它本身 **不拥有复杂渲染资源系统**，只是统一了“输出目标”的概念。

---

### 4.2 推荐类接口

    class RenderTarget
    {
    public:
        enum class Type
        {
            BackBuffer,
            Framebuffer
        };

    public:
        RenderTarget() = default;
        ~RenderTarget() = default;

        static RenderTarget BackBuffer(uint32_t width, uint32_t height);
        static RenderTarget FromFramebuffer(const Ref<Framebuffer>& framebuffer);

        void Bind() const;
        void Unbind() const;
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const;
        uint32_t GetHeight() const;

        bool IsBackBuffer() const { return m_Type == Type::BackBuffer; }
        bool IsFramebuffer() const { return m_Type == Type::Framebuffer; }

        const Ref<Framebuffer>& GetFramebuffer() const { return m_Framebuffer; }

        Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const;
        Ref<Texture2D> GetDepthAttachment() const;

    private:
        Type m_Type = Type::BackBuffer;
        Ref<Framebuffer> m_Framebuffer;

        // 仅 BackBuffer 类型使用这两个字段存储尺寸。
        // Framebuffer 类型的宽高始终从 m_Framebuffer->GetSpecification() 读取，
        // 以保证 Resize() 之后数据不过期。
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

---

## 5. 每个接口的设计意图

### 5.1 `BackBuffer(width, height)`

用于创建一个表示默认屏幕输出的 target。

用途：

- 最终显示
- preview pass 输出
- postprocess 最终 blit

为什么要传 `width/height`：

- backbuffer 没有 `Framebuffer` 对象给你查询尺寸
- `RenderTarget` 仍然需要统一提供 `GetWidth()/GetHeight()`

---

### 5.2 `FromFramebuffer(framebuffer)`

用于把一个已有的 `Framebuffer` 包装成 renderer 语义上的 render target。

用途：

- shadow map target
- forward color target
- bloom ping-pong target
- deferred gbuffer target

---

### 5.3 `Bind()`

语义：

- 如果是 backbuffer，调用 `glBindFramebuffer(GL_FRAMEBUFFER, 0)`
- 如果是 framebuffer target，调用 `Framebuffer::Bind()`

**重要约定：`Bind()` 不负责设置 viewport。**

viewport 是 pass 的职责，应由 pass 在 `Bind()` 之后显式调用：

    target.Bind();
    RenderCommand::SetViewport(0, 0, target.GetWidth(), target.GetHeight());

原因：
- 如果 `Bind()` 对 backbuffer 隐式设置 viewport 而对 framebuffer 不设置，
  两种类型的行为就不一致，pass 仍需要分支处理，抽象目的落空。
- viewport 有时需要设置为非全尺寸（如 shadow atlas 的子区域），
  隐式设置会阻碍这类用法。

设计意义：

- pass 不再需要分支判断是屏幕还是 FBO，绑定逻辑统一
- viewport 的控制权留给 pass，不隐藏在 Bind() 的副作用里

---

### 5.4 `Unbind()`

语义：

- 对两类 target 都统一解绑到默认 framebuffer

注意：

- `Bind()` 是真正重要的
- `Unbind()` 更多是接口对称性和习惯性封装

---

### 5.5（新增）`Resize()`

语义：

- 如果是 backbuffer，更新内部存储的 `m_Width / m_Height`
- 如果是 framebuffer target，转发给 `m_Framebuffer->Resize(width, height)`

用途：

- 窗口尺寸变化时，SceneRenderer 对所有 RenderTarget 调用 `Resize()`
- 无需绕过 RenderTarget 直接访问内部 Framebuffer

注意：Framebuffer 类型不存储独立的宽高字段——`GetWidth()/GetHeight()` 始终
从 `m_Framebuffer->GetSpecification()` 读取，因此 `Resize()` 后宽高自动更新，
不存在数据过期问题。

---

### 5.6 `GetColorAttachment()`

语义：

- 如果当前 target 是 framebuffer，则返回对应 color attachment
- 如果当前 target 是 backbuffer，则返回 `nullptr`

为什么 backbuffer 返回空：

- backbuffer 不是一个通过 `Texture2D` 管理的可采样 color attachment
- 它是显示目标，不是普通资源纹理

这点非常重要：

**不是所有 RenderTarget 都必须能提供可采样 color texture。**

---

### 5.7 `GetDepthAttachment()`

语义：

- framebuffer target 返回 depth attachment
- backbuffer 返回 `nullptr`

---

## 6. RenderTarget 在项目中的典型使用方式

### 6.1 ShadowPass

输入：

- `SceneData`
- light VP

输出：

- depth-only `RenderTarget`

伪代码：

    void ShadowPass::Execute(const SceneData& scene, const RenderTarget& target)
    {
        target.Bind();
        RenderCommand::SetViewport(0, 0, target.GetWidth(), target.GetHeight());
        RenderCommand::Clear(false, true, false);

        // draw scene depth...
    }

---

### 6.2 ForwardPass

输入：

- `SceneData`
- shadow map texture

输出：

- scene color `RenderTarget`

伪代码：

    void ForwardPass::Execute(const SceneData& scene, const RenderTarget& target)
    {
        target.Bind();
        RenderCommand::SetViewport(0, 0, target.GetWidth(), target.GetHeight());
        RenderCommand::Clear(true, true, false);

        // draw lit scene...
    }

---

### 6.3 TexturePreviewPass

输入：

- source texture

输出：

- backbuffer `RenderTarget`

伪代码：

    void TexturePreviewPass::Execute(const Ref<Texture2D>& texture, const RenderTarget& target)
    {
        target.Bind();
        RenderCommand::SetViewport(0, 0, target.GetWidth(), target.GetHeight());

        // draw fullscreen quad...
    }

---

### 6.4 SceneRenderer

`SceneRenderer` 正是最适合使用 RenderTarget 的地方。

例如：

    RenderTarget shadowTarget = RenderTarget::FromFramebuffer(m_ShadowPass->GetFramebuffer());
    RenderTarget sceneTarget  = RenderTarget::FromFramebuffer(m_ForwardPass->GetFramebuffer());
    RenderTarget backBuffer   = RenderTarget::BackBuffer(m_Width, m_Height);

    m_ShadowPass->Execute(scene, lightVP, shadowTarget);
    m_ForwardPass->Execute(scene, m_ShadowPass->GetDepthTexture(), lightVP, sceneTarget);

    if (m_OutputMode == SceneRendererOutput::FinalColor)
        m_TexturePreviewPass->Execute(sceneTarget.GetColorAttachment(), backBuffer);
    else
        m_TexturePreviewPass->Execute(shadowTarget.GetDepthAttachment(), backBuffer);

这时候 RenderTarget 的价值就很清楚了：

- output flow 非常清晰
- pass 不再直接绑死 `Framebuffer`
- backbuffer 和 offscreen target 在同一套语义系统里

---

## 7. 第一阶段不应该放进 RenderTarget 的东西

以下内容暂时不要放进去，否则会过度设计。

### 7.1 不要让 RenderTarget 自己创建 Framebuffer

当前阶段创建 FBO 的逻辑仍然放在 `Framebuffer` 或 pass 初始化里。

原因：

- `Framebuffer` 已经有自己的 spec 和创建逻辑
- 如果 RenderTarget 又能创建 FBO，会和 `Framebuffer` 职责重叠

---

### 7.2 不要让 RenderTarget 持有一组复杂 attachment 描述并自己管理重建

当前阶段 attachment 生命周期由 `Framebuffer` 负责。

---

### 7.3 不要在 RenderTarget 中做 render graph / dependency tracking

现在还没到这一步。

---

### 7.4 不要把 clear color / load-store ops 放进去

虽然现代图形 API 常会这么做，但你当前阶段还不需要。
这些更适合以后如果你做 render graph 或 pass descriptor 时再加。

---

### 7.5 不要在 `Bind()` 里隐式设置 viewport

`Bind()` 只做绑定，不做 viewport 设置。

原因：BackBuffer 和 Framebuffer 两种类型必须行为一致——如果其中一个在
`Bind()` 里设置了 viewport 而另一个没有，pass 代码仍需要分支处理，
统一抽象的目标就失去了意义。viewport 始终由 pass 在 `Bind()` 后显式设置。

---

## 8. 当前阶段推荐实现

### 8.1 `RenderTarget.h`

    #pragma once

    #include <cstdint>

    #include "Base.h"

    class Framebuffer;
    class Texture2D;

    class RenderTarget
    {
    public:
        enum class Type
        {
            BackBuffer,
            Framebuffer
        };

    public:
        RenderTarget() = default;
        ~RenderTarget() = default;

        static RenderTarget BackBuffer(uint32_t width, uint32_t height);
        static RenderTarget FromFramebuffer(const Ref<Framebuffer>& framebuffer);

        void Bind() const;
        void Unbind() const;
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const;
        uint32_t GetHeight() const;

        bool IsBackBuffer() const { return m_Type == Type::BackBuffer; }
        bool IsFramebuffer() const { return m_Type == Type::Framebuffer; }

        const Ref<Framebuffer>& GetFramebuffer() const { return m_Framebuffer; }

        Ref<Texture2D> GetColorAttachment(uint32_t index = 0) const;
        Ref<Texture2D> GetDepthAttachment() const;

    private:
        Type m_Type = Type::BackBuffer;
        Ref<Framebuffer> m_Framebuffer;

        // 仅 BackBuffer 类型使用。Framebuffer 类型的宽高从 spec 读取，见 GetWidth()/GetHeight()。
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

---

### 8.2 `RenderTarget.cpp`

    #include "RenderTarget.h"

    #include <cassert>

    #include <glad/glad.h>

    #include "RenderCore/Framebuffer.h"
    #include "RenderCore/RenderCommand.h"
    #include "RenderCore/Texture.h"

    RenderTarget RenderTarget::BackBuffer(uint32_t width, uint32_t height)
    {
        RenderTarget target;
        target.m_Type = Type::BackBuffer;
        target.m_Width = width;
        target.m_Height = height;
        return target;
    }

    RenderTarget RenderTarget::FromFramebuffer(const Ref<Framebuffer>& framebuffer)
    {
        assert(framebuffer && "RenderTarget::FromFramebuffer requires a valid framebuffer");

        RenderTarget target;
        target.m_Type = Type::Framebuffer;
        target.m_Framebuffer = framebuffer;
        // m_Width/m_Height 不设置：Framebuffer 类型的宽高由 GetWidth()/GetHeight()
        // 从 m_Framebuffer->GetSpecification() 实时读取，始终与 FBO 当前状态一致。
        return target;
    }

    void RenderTarget::Bind() const
    {
        if (m_Type == Type::BackBuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            // Viewport 不在这里设置——由 pass 在 Bind() 后显式调用 SetViewport，
            // 以保证 BackBuffer 和 Framebuffer 两种类型的行为完全一致。
        }
        else
        {
            assert(m_Framebuffer && "Framebuffer target is null");
            m_Framebuffer->Bind();
        }
    }

    void RenderTarget::Unbind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void RenderTarget::Resize(uint32_t width, uint32_t height)
    {
        if (m_Type == Type::BackBuffer)
        {
            m_Width  = width;
            m_Height = height;
        }
        else
        {
            assert(m_Framebuffer && "Framebuffer target is null");
            m_Framebuffer->Resize(width, height);
            // GetWidth()/GetHeight() 从 spec 读取，无需更新字段
        }
    }

    uint32_t RenderTarget::GetWidth() const
    {
        if (m_Type == Type::Framebuffer)
        {
            assert(m_Framebuffer && "Framebuffer target is null");
            return m_Framebuffer->GetSpecification().Width;
        }
        return m_Width;
    }

    uint32_t RenderTarget::GetHeight() const
    {
        if (m_Type == Type::Framebuffer)
        {
            assert(m_Framebuffer && "Framebuffer target is null");
            return m_Framebuffer->GetSpecification().Height;
        }
        return m_Height;
    }

    Ref<Texture2D> RenderTarget::GetColorAttachment(uint32_t index) const
    {
        if (m_Type == Type::Framebuffer)
        {
            assert(m_Framebuffer && "Framebuffer target is null");
            return m_Framebuffer->GetColorAttachment(index);
        }
        return nullptr;
    }

    Ref<Texture2D> RenderTarget::GetDepthAttachment() const
    {
        if (m_Type == Type::Framebuffer)
        {
            assert(m_Framebuffer && "Framebuffer target is null");
            return m_Framebuffer->GetDepthAttachment();
        }
        return nullptr;
    }

---

## 9. 为什么这个设计适合你当前项目

### 9.1 它不会破坏你现有 Framebuffer 设计

你的 `Framebuffer` 仍然是唯一真正管理 attachments 的对象。

---

### 9.2 它能立刻提升 pass 代码的语义清晰度

比起 pass 里到处直接分支：

- 是否 bind 0
- 是否有 framebuffer
- 是否读取 color attachment

统一走 `RenderTarget` 后，接口会更干净。

---

### 9.3 它给你未来扩展留足空间

将来你完全可以把 `RenderTarget` 扩展成：

- 多 target alias
- resolve target
- ping-pong pair
- typed output descriptors

但不会影响当前最小实现。

---

## 10. 第二阶段可能的扩展方向（现在先不用实现）

如果以后你项目继续发展，`RenderTarget` 可以自然扩展这些内容：

### 10.1 `RenderTargetDescriptor`

用于统一描述：

- clear color
- clear depth
- load/store behavior
- resolve behavior

### 10.2 `MultiRenderTarget`

支持：

- GBuffer position/normal/albedo
- MRT 输出

### 10.3 `BackBufferTarget` / `FramebufferTarget` 分拆成两个子类

如果以后你想让类型系统更清晰，可以走多态版本。

### 10.4 `ResolvedTarget`

用于 MSAA resolve 之后的输出目标。

但这些现在都不是必须的。

---

## 11. 结论

在当前项目里，`RenderTarget` 的设计目的可以总结为：

**它是一个 Renderer 层的“输出目标抽象”，用于统一表示 backbuffer 和 framebuffer，并把“某个 pass 的结果写到哪里”从底层 OpenGL 资源细节中抽出来。**

它不替代 `Framebuffer`，而是站在 `Framebuffer` 之上，为：

- `ShadowPass`
- `ForwardPass`
- `TexturePreviewPass`
- `SceneRenderer`

提供更清晰的输出语义。

---

## 12. 最终建议

当前项目中，推荐你采用：

- **第一阶段：薄包装 RenderTarget**
- **底层资源仍由 Framebuffer 管理**
- **Pass 和 SceneRenderer 开始改为接收/使用 RenderTarget**

这是最适合你现在 RT Rendering Lab 阶段的方案。

---

## 13. 多后端架构中的演化路径

> 本节适用于项目目标已调整为 **OpenGL / Metal / Vulkan 三后端支持**的情况。

### 13.1 当前文档描述的是 OpenGL 后端实现

第 8 节的 `RenderTarget.cpp` 包含 `#include <glad/glad.h>`，并直接调用
`glBindFramebuffer`。这意味着当前设计是 OpenGL 后端的具体实现，而非跨平台抽象。

在多后端架构下，这没有问题——只需把它的定位明确为 **OpenGL 后端的实现类**，
并把文件迁移到对应的目录即可。

---

### 13.2 迁移后的目录结构

```
src/graphics/
  interface/
    IRenderTarget.h        ← 纯虚接口（无任何平台 include）

  opengl/
    GLRenderTarget.h       ← 继承 IRenderTarget，当前 RenderTarget 代码的直接改名
    GLRenderTarget.cpp     ← 包含 glad，调用 glBindFramebuffer

  metal/
    MTLRenderTarget.h      ← 继承 IRenderTarget
    MTLRenderTarget.mm     ← 使用 MTLRenderPassDescriptor
```

---

### 13.3 抽象接口 `IRenderTarget`

```cpp
// src/graphics/interface/IRenderTarget.h
#pragma once
#include <cstdint>
#include "Base.h"

class ITexture2D;

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual void Resize(uint32_t width, uint32_t height) = 0;

    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;

    virtual bool IsBackBuffer() const = 0;

    virtual Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const = 0;
    virtual Ref<ITexture2D> GetDepthAttachment() const = 0;
};
```

所有 Pass 和 SceneRenderer 只持有 `Ref<IRenderTarget>`，完全不感知底层是哪个 API。

---

### 13.4 Metal 后端的对应概念

Metal 没有 `glBindFramebuffer`，输出目标通过 `MTLRenderPassDescriptor` 描述，
在创建 `MTLRenderCommandEncoder` 时绑定：

```objc
// MTLRenderTarget.mm (概念示意)
void MTLRenderTarget::Bind() const {
    // Metal 不在此时绑定——descriptor 在 Execute 开始时传给 commandEncoder
    // Bind() 在 Metal 后端里是"准备 descriptor"而非"调用绑定 API"
    m_RenderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    // 配置 colorAttachments[0].texture 等...
}
```

这是 Metal 与 OpenGL 最大的概念差异之一：**Metal 的"绑定"发生在 encoder 创建时，
而不是一个独立的状态设置调用**。`IRenderTarget::Bind()` 在 Metal 后端的语义
等价于"配置并持有 render pass descriptor，供 encoder 创建时使用"。

---

### 13.5 不变的部分

无论后端如何切换，以下设计决策在所有后端中保持一致：

- **`Bind()` 不设置 viewport**：viewport 始终由 Pass 在 `Bind()` 后显式调用
- **backbuffer vs offscreen 统一接口**：`IsBackBuffer()` 让 Pass 无需分支
- **`GetColorAttachment()` 在 backbuffer 时返回 nullptr**：backbuffer 不是可采样纹理
- **RenderTarget 不负责资源创建**：具体的 Framebuffer / MTLTexture 由各自的工厂创建，
  RenderTarget 只做包装