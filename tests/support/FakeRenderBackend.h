#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/Framebuffer.h"
#include "graphics/RenderTypes.h"
#include "graphics/ShaderUniformLayout.h"
#include "graphics/Texture.h"
#include "graphics/interfaces/IFramebuffer.h"
#include "graphics/interfaces/IGraphicsDevice.h"
#include "graphics/interfaces/IIndexBuffer.h"
#include "graphics/interfaces/IRenderCommand.h"
#include "graphics/interfaces/IRenderTarget.h"
#include "graphics/interfaces/IShader.h"
#include "graphics/interfaces/ITexture2D.h"
#include "graphics/interfaces/IUniformBuffer.h"
#include "graphics/interfaces/IVertexArray.h"
#include "graphics/interfaces/IVertexBuffer.h"

class FakeTexture2D final : public ITexture2D
{
public:
    explicit FakeTexture2D(const TextureSpecification &spec)
        : m_Width(spec.Width), m_Height(spec.Height), m_Format(spec.Format)
    {
    }

    uint32_t GetWidth() const override { return m_Width; }
    uint32_t GetHeight() const override { return m_Height; }
    TextureFormat GetFormat() const override { return m_Format; }

    void Bind(uint32_t slot = 0) const override { m_LastBoundSlot = slot; }
    void Unbind(uint32_t slot = 0) const override { m_LastUnboundSlot = slot; }

    void SetData(const void *data) override
    {
        m_LastSetData = data;
    }

    bool operator==(const ITexture2D &other) const override
    {
        return this == &other;
    }

    void Resize(uint32_t width, uint32_t height)
    {
        m_Width = width;
        m_Height = height;
    }

public:
    mutable uint32_t m_LastBoundSlot = 0;
    mutable uint32_t m_LastUnboundSlot = 0;
    const void *m_LastSetData = nullptr;

private:
    uint32_t m_Width = 1;
    uint32_t m_Height = 1;
    TextureFormat m_Format = TextureFormat::RGBA8;
};

class FakeFramebuffer final : public IFramebuffer
{
public:
    explicit FakeFramebuffer(const FramebufferSpecification &spec)
        : m_Specification(spec)
    {
        for (const auto &attachment : spec.Attachments.Attachments)
        {
            TextureSpecification textureSpec;
            textureSpec.Width = spec.Width;
            textureSpec.Height = spec.Height;
            textureSpec.Format = attachment.Format;

            if (attachment.Format == TextureFormat::Depth || attachment.Format == TextureFormat::Depth24Stencil8)
                m_DepthAttachment = CreateRef<FakeTexture2D>(textureSpec);
            else if (attachment.Format != TextureFormat::None)
                m_ColorAttachments.push_back(CreateRef<FakeTexture2D>(textureSpec));
        }
    }

    void Bind() const override {}
    void Unbind() const override {}

    void Resize(uint32_t width, uint32_t height) override
    {
        ++ResizeCount;
        m_Specification.Width = width;
        m_Specification.Height = height;

        for (const auto &attachment : m_ColorAttachments)
            attachment->Resize(width, height);

        if (m_DepthAttachment)
            m_DepthAttachment->Resize(width, height);
    }

    const FramebufferSpecification &GetSpecification() const override
    {
        return m_Specification;
    }

    Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const override
    {
        if (index >= m_ColorAttachments.size())
            return nullptr;
        return m_ColorAttachments[index];
    }

    Ref<ITexture2D> GetDepthAttachment() const override
    {
        return m_DepthAttachment;
    }

    int ReadPixel(uint32_t attachmentIndex, int x, int y) const override
    {
        (void)attachmentIndex;
        (void)x;
        (void)y;
        return LastClearedIntegerValue;
    }

    void ClearAttachment(uint32_t attachmentIndex, int value) override
    {
        (void)attachmentIndex;
        LastClearedIntegerValue = value;
    }

public:
    uint32_t ResizeCount = 0;
    int LastClearedIntegerValue = 0;

private:
    FramebufferSpecification m_Specification;
    std::vector<Ref<FakeTexture2D>> m_ColorAttachments;
    Ref<FakeTexture2D> m_DepthAttachment;
};

class FakeRenderTarget final : public IRenderTarget
{
public:
    static Ref<FakeRenderTarget> CreateBackBuffer(uint32_t width, uint32_t height)
    {
        auto target = CreateRef<FakeRenderTarget>();
        target->m_IsBackBuffer = true;
        target->m_Width = width;
        target->m_Height = height;
        return target;
    }

    static Ref<FakeRenderTarget> CreateFromFramebuffer(const Ref<IFramebuffer> &framebuffer)
    {
        auto target = CreateRef<FakeRenderTarget>();
        target->m_IsBackBuffer = false;
        target->m_Framebuffer = framebuffer;
        return target;
    }

    void Resize(uint32_t width, uint32_t height) override
    {
        ++ResizeCount;
        if (m_IsBackBuffer)
        {
            m_Width = width;
            m_Height = height;
            return;
        }

        m_Framebuffer->Resize(width, height);
    }

    uint32_t GetWidth() const override
    {
        return m_IsBackBuffer ? m_Width : m_Framebuffer->GetSpecification().Width;
    }

    uint32_t GetHeight() const override
    {
        return m_IsBackBuffer ? m_Height : m_Framebuffer->GetSpecification().Height;
    }

    bool IsBackBuffer() const override { return m_IsBackBuffer; }

    Ref<IFramebuffer> GetFramebuffer() const override
    {
        return m_Framebuffer;
    }

    Ref<ITexture2D> GetColorAttachment(uint32_t index = 0) const override
    {
        return m_IsBackBuffer ? nullptr : m_Framebuffer->GetColorAttachment(index);
    }

    Ref<ITexture2D> GetDepthAttachment() const override
    {
        return m_IsBackBuffer ? nullptr : m_Framebuffer->GetDepthAttachment();
    }

public:
    uint32_t ResizeCount = 0;

private:
    bool m_IsBackBuffer = true;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    Ref<IFramebuffer> m_Framebuffer;
};

class FakeShader final : public IShader
{
public:
    struct UniformBufferBindEvent
    {
        ShaderBindingPoint BindingPoint = {};
        uint32_t Slot = 0;
        Ref<IUniformBuffer> Buffer;
    };

    struct TextureBindEvent
    {
        ShaderBindingPoint BindingPoint = {};
        uint32_t Slot = 0;
        Ref<ITexture2D> Texture;
    };

    explicit FakeShader(std::string name)
        : m_Name(std::move(name))
    {
    }

    void Bind() const override {}
    void Unbind() const override {}

    const std::string &GetName() const override { return m_Name; }

    void SetInt(const std::string &name, int value) override
    {
        (void)name;
        (void)value;
    }

    void SetIntArray(const std::string &name, const int *values, uint32_t count) override
    {
        (void)name;
        (void)values;
        (void)count;
    }

    void SetBool(const std::string &name, bool value) override
    {
        (void)name;
        (void)value;
    }

    void SetFloat(const std::string &name, float value) override
    {
        (void)name;
        (void)value;
    }

    void SetFloat2(const std::string &name, const glm::vec2 &value) override
    {
        (void)name;
        (void)value;
    }

    void SetFloat3(const std::string &name, const glm::vec3 &value) override
    {
        (void)name;
        (void)value;
    }

    void SetFloat4(const std::string &name, const glm::vec4 &value) override
    {
        (void)name;
        (void)value;
    }

    void SetMat3(const std::string &name, const glm::mat3 &value) override
    {
        (void)name;
        (void)value;
    }

    void SetMat4(const std::string &name, const glm::mat4 &value) override
    {
        (void)name;
        (void)value;
    }

    void SetUniformBlock(uint32_t binding, const void *data, uint32_t size) override
    {
        LastUniformBinding = binding;
        LastUniformData = data;
        LastUniformSize = size;
        ++UniformUploadCount;
        LastUniformBytes.resize(size);

        if (size > 0 && data)
            std::memcpy(LastUniformBytes.data(), data, size);
    }

    void BindUniformBuffer(ShaderBindingPoint binding, const Ref<IUniformBuffer> &buffer) override
    {
        if (buffer)
            LogicalBoundUniformBuffers[binding] = buffer;
        else
            LogicalBoundUniformBuffers.erase(binding);

        if (binding.Set == 0)
        {
            if (buffer)
                BoundUniformBuffers[binding.Binding] = buffer;
            else
                BoundUniformBuffers.erase(binding.Binding);
        }

        UniformBufferBindEvents.push_back({binding, binding.Binding, buffer});
    }

    void BindTexture(ShaderBindingPoint binding, const Ref<ITexture2D> &texture) override
    {
        if (texture)
            LogicalBoundTextures[binding] = texture;
        else
            LogicalBoundTextures.erase(binding);

        if (binding.Set == 0)
        {
            if (texture)
                BoundTextures[binding.Binding] = texture;
            else
                BoundTextures.erase(binding.Binding);
        }

        TextureBindEvents.push_back({binding, binding.Binding, texture});
    }

    const ShaderUniformBlockLayout *GetUniformBlockLayout(ShaderBindingPoint binding) const override
    {
        auto logicalIt = LogicalBlockLayouts.find(binding);
        if (logicalIt != LogicalBlockLayouts.end())
            return &logicalIt->second;

        if (binding.Set != 0)
            return nullptr;

        auto flatIt = BlockLayouts.find(binding.Binding);
        if (flatIt == BlockLayouts.end())
            return nullptr;

        return &flatIt->second;
    }

public:
    uint32_t LastUniformBinding = 0;
    const void *LastUniformData = nullptr;
    uint32_t LastUniformSize = 0;
    uint32_t UniformUploadCount = 0;
    std::vector<std::byte> LastUniformBytes;
    std::unordered_map<uint32_t, ShaderUniformBlockLayout> BlockLayouts;
    std::unordered_map<ShaderBindingPoint, ShaderUniformBlockLayout, ShaderBindingPointHash> LogicalBlockLayouts;
    std::unordered_map<uint32_t, Ref<IUniformBuffer>> BoundUniformBuffers;
    std::unordered_map<ShaderBindingPoint, Ref<IUniformBuffer>, ShaderBindingPointHash> LogicalBoundUniformBuffers;
    std::vector<UniformBufferBindEvent> UniformBufferBindEvents;
    std::unordered_map<uint32_t, Ref<ITexture2D>> BoundTextures;
    std::unordered_map<ShaderBindingPoint, Ref<ITexture2D>, ShaderBindingPointHash> LogicalBoundTextures;
    std::vector<TextureBindEvent> TextureBindEvents;

private:
    std::string m_Name;
};

class FakeVertexBuffer final : public IVertexBuffer
{
public:
    explicit FakeVertexBuffer(uint32_t size)
        : m_Size(size)
    {
    }

    void SetData(const void *data, uint32_t size, uint32_t offset = 0) override
    {
        LastSetData = data;
        LastSetSize = size;
        LastSetOffset = offset;
    }

    void SetLayout(const BufferLayout &layout) override
    {
        m_Layout = layout;
    }

    const BufferLayout &GetLayout() const override
    {
        return m_Layout;
    }

public:
    uint32_t m_Size = 0;
    const void *LastSetData = nullptr;
    uint32_t LastSetSize = 0;
    uint32_t LastSetOffset = 0;

private:
    BufferLayout m_Layout;
};

class FakeIndexBuffer final : public IIndexBuffer
{
public:
    explicit FakeIndexBuffer(uint32_t count)
        : m_Count(count)
    {
    }

    uint32_t GetCount() const override { return m_Count; }

private:
    uint32_t m_Count = 0;
};

class FakeUniformBuffer final : public IUniformBuffer
{
public:
    explicit FakeUniformBuffer(uint32_t size)
        : m_Size(size), Bytes(size)
    {
    }

    void SetData(const void *data, uint32_t size, uint32_t offset = 0) override
    {
        LastSetData = data;
        LastSetSize = size;
        LastSetOffset = offset;

        if (offset + size > Bytes.size())
            Bytes.resize(offset + size);

        if (size > 0 && data)
            std::memcpy(Bytes.data() + offset, data, size);
    }

    uint32_t GetSize() const override
    {
        return m_Size;
    }

public:
    uint32_t m_Size = 0;
    const void *LastSetData = nullptr;
    uint32_t LastSetSize = 0;
    uint32_t LastSetOffset = 0;
    std::vector<std::byte> Bytes;
};

class FakeVertexArray final : public IVertexArray
{
public:
    void Bind() const override {}
    void Unbind() const override {}

    void AddVertexBuffer(const Ref<IVertexBuffer> &vb) override
    {
        m_VertexBuffers.push_back(vb);
    }

    void SetIndexBuffer(const Ref<IIndexBuffer> &ib) override
    {
        m_IndexBuffer = ib;
    }

    const Ref<IIndexBuffer> &GetIndexBuffer() const override
    {
        return m_IndexBuffer;
    }

private:
    std::vector<Ref<IVertexBuffer>> m_VertexBuffers;
    Ref<IIndexBuffer> m_IndexBuffer;
};

class FakeRenderCommand final : public IRenderCommand
{
public:
    struct BeginRenderPassEvent
    {
        Ref<IRenderTarget> Target;
        RenderPassDescriptor Descriptor;
    };

    struct TextureBindEvent
    {
        uint32_t Slot = 0;
        Ref<ITexture2D> Texture;
        int PassIndex = -1;
    };

    struct DrawIndexedEvent
    {
        Ref<IVertexArray> VertexArray;
        uint32_t IndexCount = 0;
        int PassIndex = -1;
    };

    struct ViewportEvent
    {
        uint32_t X = 0;
        uint32_t Y = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        int PassIndex = -1;
    };

    void Init() override {}

    void BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc) override
    {
        BeginRenderPasses.push_back({target, desc});
        m_CurrentPassIndex = static_cast<int>(BeginRenderPasses.size()) - 1;
    }

    void EndRenderPass() override
    {
        ++EndRenderPassCount;
    }

    void SetPipelineState(const PipelineState &state) override
    {
        PipelineStates.push_back(state);
    }

    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override
    {
        Viewports.push_back({x, y, width, height, m_CurrentPassIndex});
    }

    void SetTexture(uint32_t slot, const Ref<ITexture2D> &texture) override
    {
        TextureBinds.push_back({slot, texture, m_CurrentPassIndex});
    }

    void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0) override
    {
        DrawIndexedCalls.push_back({vao, indexCount, m_CurrentPassIndex});
    }

    void DrawArrays(uint32_t mode, uint32_t first, uint32_t count) override
    {
        (void)mode;
        (void)first;
        (void)count;
    }

    void Reset()
    {
        BeginRenderPasses.clear();
        TextureBinds.clear();
        DrawIndexedCalls.clear();
        Viewports.clear();
        PipelineStates.clear();
        EndRenderPassCount = 0;
        m_CurrentPassIndex = -1;
    }

public:
    std::vector<BeginRenderPassEvent> BeginRenderPasses;
    std::vector<TextureBindEvent> TextureBinds;
    std::vector<DrawIndexedEvent> DrawIndexedCalls;
    std::vector<ViewportEvent> Viewports;
    std::vector<PipelineState> PipelineStates;
    uint32_t EndRenderPassCount = 0;

private:
    int m_CurrentPassIndex = -1;
};

class FakeGraphicsDevice final : public IGraphicsDevice
{
public:
    FakeGraphicsDevice()
        : RenderCommand(CreateRef<FakeRenderCommand>())
    {
    }

    Ref<IVertexBuffer> CreateVertexBuffer(uint32_t size, BufferUsage usage = BufferUsage::DynamicDraw) override
    {
        (void)usage;
        return CreateRef<FakeVertexBuffer>(size);
    }

    Ref<IVertexBuffer> CreateVertexBuffer(const void *data, uint32_t size, BufferUsage usage = BufferUsage::StaticDraw) override
    {
        (void)usage;
        auto buffer = CreateRef<FakeVertexBuffer>(size);
        buffer->SetData(data, size);
        return buffer;
    }

    Ref<IIndexBuffer> CreateIndexBuffer(const uint32_t *indices, uint32_t count) override
    {
        (void)indices;
        return CreateRef<FakeIndexBuffer>(count);
    }

    Ref<IUniformBuffer> CreateUniformBuffer(uint32_t size) override
    {
        auto buffer = CreateRef<FakeUniformBuffer>(size);
        CreatedUniformBuffers.push_back(buffer);
        return buffer;
    }

    Ref<IVertexArray> CreateVertexArray() override
    {
        return CreateRef<FakeVertexArray>();
    }

    Ref<ITexture2D> CreateTexture2D(const TextureSpecification &spec) override
    {
        auto texture = CreateRef<FakeTexture2D>(spec);
        CreatedTextures.push_back(texture);
        return texture;
    }

    Ref<ITexture2D> CreateTexture2DFromFile(const std::string &path, bool flipVertically = true) override
    {
        (void)path;
        (void)flipVertically;
        TextureSpecification spec;
        return CreateTexture2D(spec);
    }

    Ref<IShader> CreateShader(const std::string &name) override
    {
        auto shader = CreateRef<FakeShader>(name);
        PopulateShaderLayouts(*shader);
        CreatedShaders.push_back(shader);
        return shader;
    }

    Ref<IShader> CreateShaderFromSource(const std::string &name, const std::string &vertexSrc, const std::string &fragmentSrc, const std::string &geometrySrc = "") override
    {
        (void)vertexSrc;
        (void)fragmentSrc;
        (void)geometrySrc;
        return CreateShader(name);
    }

    Ref<IShader> CreateShaderFromFiles(const std::string &name, const std::string &vertexPath, const std::string &fragmentPath, const std::string &geometryPath = "") override
    {
        (void)vertexPath;
        (void)fragmentPath;
        (void)geometryPath;
        return CreateShader(name);
    }

    Ref<IFramebuffer> CreateFramebuffer(const FramebufferSpecification &spec) override
    {
        auto framebuffer = CreateRef<FakeFramebuffer>(spec);
        CreatedFramebuffers.push_back(framebuffer);
        return framebuffer;
    }

    Ref<IRenderTarget> CreateRenderTargetBackBuffer(uint32_t width, uint32_t height) override
    {
        auto target = FakeRenderTarget::CreateBackBuffer(width, height);
        BackBufferTargets.push_back(target);
        LastBackBufferTarget = target;
        return target;
    }

    Ref<IRenderTarget> CreateRenderTargetFromFramebuffer(const Ref<IFramebuffer> &fb) override
    {
        auto target = FakeRenderTarget::CreateFromFramebuffer(fb);
        FramebufferTargets.push_back(target);
        return target;
    }

    Ref<IRenderCommand> GetRenderCommand() override
    {
        return RenderCommand;
    }

public:
    Ref<FakeRenderCommand> RenderCommand;
    Ref<FakeRenderTarget> LastBackBufferTarget;
    std::vector<Ref<FakeTexture2D>> CreatedTextures;
    std::vector<Ref<FakeShader>> CreatedShaders;
    std::vector<Ref<FakeUniformBuffer>> CreatedUniformBuffers;
    std::vector<Ref<FakeFramebuffer>> CreatedFramebuffers;
    std::vector<Ref<FakeRenderTarget>> BackBufferTargets;
    std::vector<Ref<FakeRenderTarget>> FramebufferTargets;

private:
    static void AddField(ShaderUniformBlockLayout &layout,
                         const std::string &name,
                         uint32_t offset,
                         uint32_t size,
                         ShaderUniformValueType type = ShaderUniformValueType::Unknown)
    {
        layout.AddField({name, offset, size, type});
    }

    static void PopulateShaderLayouts(FakeShader &shader)
    {
        if (shader.GetName() == "BasicLit")
        {
            ShaderUniformBlockLayout layout("GlobalParams", 0, 272);
            AddField(layout, "u_ViewProjection", 0, 64, ShaderUniformValueType::Mat4);
            AddField(layout, "u_Model", 64, 64, ShaderUniformValueType::Mat4);
            AddField(layout, "u_NormalMatrix", 128, 64, ShaderUniformValueType::Mat4);
            AddField(layout, "u_CameraPosition", 192, 12, ShaderUniformValueType::Float3);
            AddField(layout, "u_LightDirection", 208, 12, ShaderUniformValueType::Float3);
            AddField(layout, "u_LightColor", 224, 12, ShaderUniformValueType::Float3);
            AddField(layout, "u_LightIntensity", 236, 4, ShaderUniformValueType::Float);
            AddField(layout, "u_Albedo", 240, 12, ShaderUniformValueType::Float3);
            AddField(layout, "u_SpecularPower", 252, 4, ShaderUniformValueType::Float);
            AddField(layout, "u_AmbientStrength", 256, 4, ShaderUniformValueType::Float);
            shader.BlockLayouts[0] = std::move(layout);
            shader.LogicalBlockLayouts[MakeFlatShaderBindingPoint(0)] = shader.BlockLayouts.at(0);
            return;
        }

        if (shader.GetName() == "ForwardLit")
        {
            ShaderUniformBlockLayout perPass("FramePassParams", ShaderBindingPoints::PerFrame, 192);
            AddField(perPass, "u_ViewProjection", 0, 64, ShaderUniformValueType::Mat4);
            AddField(perPass, "u_LightViewProjection", 64, 64, ShaderUniformValueType::Mat4);
            AddField(perPass, "u_CameraPosition", 128, 12, ShaderUniformValueType::Float3);
            AddField(perPass, "u_LightDirection", 144, 12, ShaderUniformValueType::Float3);
            AddField(perPass, "u_LightColor", 160, 12, ShaderUniformValueType::Float3);
            AddField(perPass, "u_LightIntensity", 172, 4, ShaderUniformValueType::Float);
            AddField(perPass, "u_ShadowMapTexelSize", 176, 8, ShaderUniformValueType::Float2);
            shader.BlockLayouts[FlattenShaderBindingPointForOpenGL(ShaderBindingPoints::PerFrame)] = perPass;
            shader.LogicalBlockLayouts[ShaderBindingPoints::PerFrame] = std::move(perPass);

            ShaderUniformBlockLayout perMaterial("MaterialParams", ShaderBindingPoints::PerMaterial, 32);
            AddField(perMaterial, "u_Albedo", 0, 12, ShaderUniformValueType::Float3);
            AddField(perMaterial, "u_SpecularPower", 12, 4, ShaderUniformValueType::Float);
            AddField(perMaterial, "u_AmbientStrength", 16, 4, ShaderUniformValueType::Float);
            AddField(perMaterial, "u_UseAlbedoMap", 20, 4, ShaderUniformValueType::Bool);
            shader.BlockLayouts[FlattenShaderBindingPointForOpenGL(ShaderBindingPoints::PerMaterial)] = perMaterial;
            shader.LogicalBlockLayouts[ShaderBindingPoints::PerMaterial] = std::move(perMaterial);

            ShaderUniformBlockLayout perDraw("DrawParams", ShaderBindingPoints::PerDraw, 128);
            AddField(perDraw, "u_Model", 0, 64, ShaderUniformValueType::Mat4);
            AddField(perDraw, "u_NormalMatrix", 64, 64, ShaderUniformValueType::Mat4);
            shader.BlockLayouts[FlattenShaderBindingPointForOpenGL(ShaderBindingPoints::PerDraw)] = perDraw;
            shader.LogicalBlockLayouts[ShaderBindingPoints::PerDraw] = std::move(perDraw);
            return;
        }

        if (shader.GetName() == "ShadowDepth")
        {
            ShaderUniformBlockLayout layout("GlobalParams", 0, 128);
            AddField(layout, "u_LightViewProjection", 0, 64, ShaderUniformValueType::Mat4);
            AddField(layout, "u_Model", 64, 64, ShaderUniformValueType::Mat4);
            shader.BlockLayouts[0] = std::move(layout);
            shader.LogicalBlockLayouts[MakeFlatShaderBindingPoint(0)] = shader.BlockLayouts.at(0);
            return;
        }

        if (shader.GetName() == "TexturePreview")
        {
            ShaderUniformBlockLayout layout("GlobalParams", 0, 4);
            AddField(layout, "u_IsDepthTexture", 0, 4, ShaderUniformValueType::Bool);
            shader.BlockLayouts[0] = std::move(layout);
            shader.LogicalBlockLayouts[MakeFlatShaderBindingPoint(0)] = shader.BlockLayouts.at(0);
        }
    }
};
