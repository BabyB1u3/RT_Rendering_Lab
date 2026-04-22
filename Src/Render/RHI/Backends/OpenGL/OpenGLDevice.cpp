#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
GLenum ToGLBufferUsage(MemoryUsage memoryUsage)
{
    switch (memoryUsage)
    {
        case MemoryUsage::GpuOnly:
            return GL_STATIC_DRAW;
        case MemoryUsage::CpuToGpu:
            return GL_DYNAMIC_DRAW;
        case MemoryUsage::GpuToCpu:
            return GL_STREAM_READ;
    }

    return GL_STATIC_DRAW;
}

GLenum ToGLTarget(TextureType type)
{
    switch (type)
    {
        case TextureType::Tex2D:
            return GL_TEXTURE_2D;
        case TextureType::Tex2DArray:
            return GL_TEXTURE_2D_ARRAY;
        case TextureType::Tex3D:
            return GL_TEXTURE_3D;
        case TextureType::Cube:
            return GL_TEXTURE_CUBE_MAP;
    }

    return GL_TEXTURE_2D;
}

GLenum ToGLInternalFormat(Format format)
{
    switch (format)
    {
        case Format::R8_UNORM:
            return GL_R8;
        case Format::RG8_UNORM:
            return GL_RG8;
        case Format::RGBA8_UNORM:
            return GL_RGBA8;
        case Format::RGBA8_SRGB:
            return GL_SRGB8_ALPHA8;
        case Format::BGRA8_UNORM:
        case Format::BGRA8_SRGB:
            return GL_RGBA8;
        case Format::R16F:
            return GL_R16F;
        case Format::RG16F:
            return GL_RG16F;
        case Format::RGBA16F:
            return GL_RGBA16F;
        case Format::R32F:
            return GL_R32F;
        case Format::RG32F:
            return GL_RG32F;
        case Format::RGBA32F:
            return GL_RGBA32F;
        case Format::R32_UINT:
            return GL_R32UI;
        case Format::D16_UNORM:
            return GL_DEPTH_COMPONENT16;
        case Format::D32_SFLOAT:
            return GL_DEPTH_COMPONENT32F;
        case Format::D24_UNORM_S8_UINT:
            return GL_DEPTH24_STENCIL8;
        case Format::D32_SFLOAT_S8_UINT:
            return GL_DEPTH32F_STENCIL8;
        default:
            RTRLAB_ASSERTF(false, "Unsupported OpenGL RHI format {}", static_cast<uint32_t>(format));
            return GL_RGBA8;
    }
}

GLenum ToGLFilter(FilterMode mode)
{
    return mode == FilterMode::Nearest ? GL_NEAREST : GL_LINEAR;
}

GLenum ToGLMinFilter(FilterMode minFilter, MipFilterMode mipFilter)
{
    if (mipFilter == MipFilterMode::None)
        return ToGLFilter(minFilter);

    if (minFilter == FilterMode::Nearest)
        return mipFilter == MipFilterMode::Linear ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;

    return mipFilter == MipFilterMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;
}

GLenum ToGLAddressMode(AddressMode mode)
{
    switch (mode)
    {
        case AddressMode::Repeat:
            return GL_REPEAT;
        case AddressMode::MirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:
            return GL_CLAMP_TO_EDGE;
        case AddressMode::ClampToBorder:
            return GL_CLAMP_TO_BORDER;
    }

    return GL_REPEAT;
}

bool HasDebugName(const char* debugName)
{
    return debugName != nullptr && debugName[0] != '\0';
}

// TRANSITIONAL(M3): OpenGL capability checks are still queried ad hoc at the
// call site. The longer-term shape is a backend-local OpenGLCapabilities /
// OpenGLExtensions cache built during device initialization, covering debug
// labels, texture views, DSA, anisotropy, and similar feature gates in one
// place. Until that exists, keep this helper as the single compatibility check
// for object labels across different GLAD generation configurations.
bool HasOpenGLObjectLabelSupport()
{
#if defined(GLAD_GL_KHR_debug)
    return GLAD_GL_VERSION_4_3 || GLAD_GL_KHR_debug;
#else
    return GLAD_GL_VERSION_4_3;
#endif
}

void SetOpenGLObjectLabel(GLenum identifier, GLuint object, const char* debugName)
{
    if (object == 0 || !HasDebugName(debugName))
        return;

    if (HasOpenGLObjectLabelSupport())
        glObjectLabel(identifier, object, -1, debugName);
}

std::string MakeTextureViewDebugName(const Texture& texture)
{
    const char* debugName = texture.GetDesc().m_DebugName;
    if (!HasDebugName(debugName))
        return {};

    return std::string(debugName) + ".View";
}

class OpenGLBuffer final : public Buffer
{
public:
    OpenGLBuffer(GLuint buffer, const BufferDesc& desc) : m_Buffer(buffer), m_Desc(desc) {}

    ~OpenGLBuffer() override
    {
        if (m_Buffer != 0)
            glDeleteBuffers(1, &m_Buffer);
    }

    const BufferDesc& GetDesc() const override { return m_Desc; }
    GLuint GetBuffer() const { return m_Buffer; }

private:
    GLuint m_Buffer = 0;
    BufferDesc m_Desc;
};

class OpenGLTexture final : public Texture
{
public:
    OpenGLTexture(GLuint texture, GLenum target, const TextureDesc& desc)
        : m_Texture(texture), m_Target(target), m_Desc(desc)
    {
    }

    ~OpenGLTexture() override
    {
        if (m_Texture != 0)
            glDeleteTextures(1, &m_Texture);
    }

    const TextureDesc& GetDesc() const override { return m_Desc; }
    GLuint GetTexture() const { return m_Texture; }
    GLenum GetTarget() const { return m_Target; }

private:
    GLuint m_Texture = 0;
    GLenum m_Target = GL_TEXTURE_2D;
    TextureDesc m_Desc;
};

class OpenGLTextureView final : public TextureView
{
public:
    OpenGLTextureView(Texture* texture, GLuint textureView, GLenum target, const TextureViewDesc& desc)
        : m_Texture(texture), m_TextureView(textureView), m_Target(target), m_Desc(desc)
    {
    }

    ~OpenGLTextureView() override
    {
        if (m_TextureView != 0)
            glDeleteTextures(1, &m_TextureView);
    }

    Texture* GetTexture() const override { return m_Texture; }
    const TextureViewDesc& GetDesc() const override { return m_Desc; }
    GLuint GetTextureView() const { return m_TextureView; }
    GLenum GetTarget() const { return m_Target; }

private:
    Texture* m_Texture = nullptr;
    GLuint m_TextureView = 0;
    GLenum m_Target = GL_TEXTURE_2D;
    TextureViewDesc m_Desc;
};

class OpenGLSampler final : public Sampler
{
public:
    OpenGLSampler(GLuint sampler, const SamplerDesc& desc) : m_Sampler(sampler), m_Desc(desc) {}

    ~OpenGLSampler() override
    {
        if (m_Sampler != 0)
            glDeleteSamplers(1, &m_Sampler);
    }

    const SamplerDesc& GetDesc() const override { return m_Desc; }
    GLuint GetSampler() const { return m_Sampler; }

private:
    GLuint m_Sampler = 0;
    SamplerDesc m_Desc;
};

struct OpenGLBindingMapEntry
{
    ResourceKind m_Kind = ResourceKind::UniformBuffer;
    uint32_t m_SetIndex = 0;
    uint32_t m_LogicalBinding = 0;
    std::string m_Name;
    uint32_t m_GlBindingPoint = 0;
    uint32_t m_GlTextureUnit = 0;
};

class OpenGLShaderProgram final : public ShaderProgram
{
public:
    OpenGLShaderProgram(GLuint program, const CompiledShaderProgramDesc& desc)
        : m_Program(program), m_Reflection(desc.m_Reflection)
    {
    }

    ~OpenGLShaderProgram() override
    {
        if (m_Program != 0)
            glDeleteProgram(m_Program);
    }

    const ShaderReflectionData& GetReflection() const override { return m_Reflection; }
    PipelineLayoutDesc DerivePipelineLayoutDesc() const override
    {
        return RHIInternal::BuildPipelineLayoutDescFromReflection(m_Reflection);
    }
    GLuint GetProgram() const { return m_Program; }

private:
    GLuint m_Program = 0;
    ShaderReflectionData m_Reflection;
};

class OpenGLResourceSet final : public ResourceSet
{
public:
    OpenGLResourceSet(PipelineLayout* layout, uint32_t setIndex) : m_Layout(layout), m_SetIndex(setIndex)
    {
        RTRLAB_ASSERT_MSG(m_Layout != nullptr, "OpenGL ResourceSet creation requires a valid PipelineLayout.");

        const std::vector<const BindingInfo*> setBindings =
            RHIInternal::CollectBindingInfosForSet(m_Layout->GetDesc(), m_SetIndex);
        RTRLAB_ASSERTF(!setBindings.empty(),
                       "OpenGL ResourceSet set {} does not exist in the provided PipelineLayout.",
                       m_SetIndex);
    }

    ~OpenGLResourceSet() override
    {
        if (m_ConstantBuffer != 0)
            glDeleteBuffers(1, &m_ConstantBuffer);
    }

    PipelineLayout* GetLayout() const override { return m_Layout; }
    uint32_t GetSetIndex() const override { return m_SetIndex; }

    const ParameterBlockData& GetConstants() const override { return m_Constants; }
    void SetConstantDataRaw(uint32_t offset, const void* data, size_t size) override
    {
        if (size == 0)
            return;

        ValidateConstantBindingExists();
        m_Constants.SetRaw(offset, data, size);
        UploadConstantData();
        ++m_Version;
    }

    void SetBuffer(uint32_t binding, const BufferBinding& bufferBinding) override
    {
        (void)RequireBindingInfo(binding, ResourceKind::StorageBuffer);
        m_BufferBindings[binding] = bufferBinding;
        ++m_Version;
    }

    void SetTexture(uint32_t binding, const TextureBinding& textureBinding) override
    {
        const BindingInfo* bindingInfo =
            RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::SampledTexture);
        if (bindingInfo == nullptr)
            bindingInfo =
                RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, ResourceKind::StorageTexture);

        RTRLAB_ASSERTF(bindingInfo != nullptr,
                       "OpenGL ResourceSet set {} has no texture binding {} in its PipelineLayout.",
                       m_SetIndex,
                       binding);
        m_TextureBindings[binding] = textureBinding;
        ++m_Version;
    }

    void SetSampler(uint32_t binding, const SamplerBinding& samplerBinding) override
    {
        (void)RequireBindingInfo(binding, ResourceKind::Sampler);
        m_SamplerBindings[binding] = samplerBinding;
        ++m_Version;
    }

    uint32_t GetVersion() const override { return m_Version; }

    bool HasConstantBinding() const
    {
        return RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer) !=
               nullptr;
    }

    GLuint GetConstantBuffer() const { return m_ConstantBuffer; }
    const BufferBinding* FindBufferBinding(uint32_t binding) const
    {
        const auto it = m_BufferBindings.find(binding);
        return it != m_BufferBindings.end() ? &it->second : nullptr;
    }
    const TextureBinding* FindTextureBinding(uint32_t binding) const
    {
        const auto it = m_TextureBindings.find(binding);
        return it != m_TextureBindings.end() ? &it->second : nullptr;
    }
    const SamplerBinding* FindSamplerBinding(uint32_t binding) const
    {
        const auto it = m_SamplerBindings.find(binding);
        return it != m_SamplerBindings.end() ? &it->second : nullptr;
    }

private:
    const BindingInfo& RequireBindingInfo(uint32_t binding, ResourceKind kind) const
    {
        RTRLAB_ASSERT_MSG(m_Layout != nullptr,
                          "OpenGL ResourceSet binding validation requires a valid PipelineLayout.");
        const BindingInfo* bindingInfo = RHIInternal::FindBindingInfo(m_Layout->GetDesc(), m_SetIndex, binding, kind);
        RTRLAB_ASSERTF(bindingInfo != nullptr,
                       "OpenGL ResourceSet set {} has no binding {} of expected kind {} in its PipelineLayout.",
                       m_SetIndex,
                       binding,
                       static_cast<uint32_t>(kind));
        return *bindingInfo;
    }

    void ValidateConstantBindingExists() const
    {
        RTRLAB_ASSERT_MSG(m_Layout != nullptr,
                          "OpenGL ResourceSet constant validation requires a valid PipelineLayout.");
        const BindingInfo* bindingInfo =
            RHIInternal::FindFirstBindingInfoForSet(m_Layout->GetDesc(), m_SetIndex, ResourceKind::UniformBuffer);
        RTRLAB_ASSERTF(bindingInfo != nullptr,
                       "OpenGL ResourceSet set {} has no UniformBuffer binding in its PipelineLayout.",
                       m_SetIndex);
    }

    void EnsureConstantBufferCapacity(size_t requiredSize)
    {
        const GLsizeiptr requiredCapacity = static_cast<GLsizeiptr>(std::max<size_t>(requiredSize, 1));
        if (m_ConstantBuffer == 0)
            glCreateBuffers(1, &m_ConstantBuffer);

        if (m_ConstantBufferCapacity >= requiredCapacity)
            return;

        glNamedBufferData(m_ConstantBuffer, requiredCapacity, nullptr, GL_DYNAMIC_DRAW);
        m_ConstantBufferCapacity = requiredCapacity;
    }

    void UploadConstantData()
    {
        EnsureConstantBufferCapacity(m_Constants.GetSize());

        const size_t byteCount = std::max<size_t>(m_Constants.GetSize(), 1);
        if (m_Constants.GetData() != nullptr)
        {
            glNamedBufferSubData(
                m_ConstantBuffer, 0, static_cast<GLsizeiptr>(m_Constants.GetSize()), m_Constants.GetData());
        }
        else
        {
            static constexpr uint8_t kZero = 0;
            glNamedBufferSubData(m_ConstantBuffer, 0, static_cast<GLsizeiptr>(byteCount), &kZero);
        }
    }

    PipelineLayout* m_Layout = nullptr;
    uint32_t m_SetIndex = 0;
    ParameterBlockData m_Constants;
    std::unordered_map<uint32_t, BufferBinding> m_BufferBindings;
    std::unordered_map<uint32_t, TextureBinding> m_TextureBindings;
    std::unordered_map<uint32_t, SamplerBinding> m_SamplerBindings;
    GLuint m_ConstantBuffer = 0;
    GLsizeiptr m_ConstantBufferCapacity = 0;
    uint32_t m_Version = 0;
};

class OpenGLVertexInputLayout final : public VertexInputLayout
{
public:
    explicit OpenGLVertexInputLayout(const VertexInputLayoutDesc& desc) : m_Desc(desc) {}

    const VertexInputLayoutDesc& GetDesc() const override { return m_Desc; }

private:
    VertexInputLayoutDesc m_Desc;
};

class OpenGLGraphicsPipeline final : public GraphicsPipeline
{
public:
    OpenGLGraphicsPipeline(GLuint program,
                           GLuint vertexArray,
                           const GraphicsPipelineDesc& desc,
                           std::vector<OpenGLBindingMapEntry>&& bindingMap)
        : m_Program(program), m_VertexArray(vertexArray), m_Desc(desc), m_BindingMap(std::move(bindingMap))
    {
    }

    ~OpenGLGraphicsPipeline() override
    {
        if (m_VertexArray != 0)
            glDeleteVertexArrays(1, &m_VertexArray);
    }

    const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
    GLuint GetProgram() const { return m_Program; }
    GLuint GetVertexArray() const { return m_VertexArray; }
    const std::vector<OpenGLBindingMapEntry>& GetBindingMap() const { return m_BindingMap; }

private:
    GLuint m_Program = 0;
    GLuint m_VertexArray = 0;
    GraphicsPipelineDesc m_Desc;
    std::vector<OpenGLBindingMapEntry> m_BindingMap;
};

GLenum ToGLShaderStage(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return GL_VERTEX_SHADER;
        case ShaderStage::Fragment:
            return GL_FRAGMENT_SHADER;
        case ShaderStage::Compute:
            return GL_COMPUTE_SHADER;
        case ShaderStage::None:
        case ShaderStage::All:
            break;
    }

    RTRLAB_ASSERTF(false, "Unsupported OpenGL shader stage {}", static_cast<uint32_t>(stage));
    return GL_VERTEX_SHADER;
}

GLenum ToGLIndexType(IndexType indexType)
{
    switch (indexType)
    {
        case IndexType::UInt16:
            return GL_UNSIGNED_SHORT;
        case IndexType::UInt32:
            return GL_UNSIGNED_INT;
    }

    return GL_UNSIGNED_INT;
}

GLenum ToGLPrimitiveTopology(PrimitiveTopology topology)
{
    switch (topology)
    {
        case PrimitiveTopology::TriangleList:
            return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip:
            return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::LineList:
            return GL_LINES;
        case PrimitiveTopology::LineStrip:
            return GL_LINE_STRIP;
        case PrimitiveTopology::PointList:
            return GL_POINTS;
    }

    return GL_TRIANGLES;
}

GLint GetOpenGLAttributeComponentCount(Format format)
{
    switch (format)
    {
        case Format::RG32F:
            return 2;
        case Format::RGBA32F:
            return 4;
        default:
            RTRLAB_ASSERTF(false, "Unsupported OpenGL vertex attribute format {}", static_cast<uint32_t>(format));
            return 0;
    }
}

const OpenGLShaderProgram& GetOpenGLShaderProgram(ShaderProgram* shaderProgram)
{
    auto* openGLShaderProgram = dynamic_cast<OpenGLShaderProgram*>(shaderProgram);
    RTRLAB_ASSERT_MSG(openGLShaderProgram != nullptr, "GraphicsPipeline requires an OpenGL shader program.");
    return *openGLShaderProgram;
}

const OpenGLVertexInputLayout& GetOpenGLVertexInputLayout(VertexInputLayout* vertexInputLayout)
{
    auto* openGLVertexInputLayout = dynamic_cast<OpenGLVertexInputLayout*>(vertexInputLayout);
    RTRLAB_ASSERT_MSG(openGLVertexInputLayout != nullptr, "GraphicsPipeline requires an OpenGL vertex input layout.");
    return *openGLVertexInputLayout;
}

const OpenGLGraphicsPipeline& GetOpenGLGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
    auto* openGLGraphicsPipeline = dynamic_cast<OpenGLGraphicsPipeline*>(graphicsPipeline);
    RTRLAB_ASSERT_MSG(openGLGraphicsPipeline != nullptr, "Graphics pipeline is not owned by the OpenGL backend.");
    return *openGLGraphicsPipeline;
}

OpenGLBuffer& GetOpenGLBuffer(Buffer* buffer)
{
    auto* openGLBuffer = dynamic_cast<OpenGLBuffer*>(buffer);
    RTRLAB_ASSERT_MSG(openGLBuffer != nullptr, "Buffer is not owned by the OpenGL backend.");
    return *openGLBuffer;
}

OpenGLResourceSet& GetOpenGLResourceSet(ResourceSet* resourceSet)
{
    auto* openGLResourceSet = dynamic_cast<OpenGLResourceSet*>(resourceSet);
    RTRLAB_ASSERT_MSG(openGLResourceSet != nullptr, "ResourceSet is not owned by the OpenGL backend.");
    return *openGLResourceSet;
}

const OpenGLTexture& GetOpenGLTexture(Texture* texture)
{
    auto* openGLTexture = dynamic_cast<OpenGLTexture*>(texture);
    RTRLAB_ASSERT_MSG(openGLTexture != nullptr, "Texture is not owned by the OpenGL backend.");
    return *openGLTexture;
}

const OpenGLTextureView* TryGetOpenGLTextureView(TextureView* textureView)
{
    return dynamic_cast<OpenGLTextureView*>(textureView);
}

const OpenGLSampler& GetOpenGLSampler(Sampler* sampler)
{
    auto* openGLSampler = dynamic_cast<OpenGLSampler*>(sampler);
    RTRLAB_ASSERT_MSG(openGLSampler != nullptr, "Sampler is not owned by the OpenGL backend.");
    return *openGLSampler;
}

std::vector<OpenGLBindingMapEntry> BuildOpenGLBindingMap(const PipelineLayoutDesc& desc)
{
    std::vector<OpenGLBindingMapEntry> bindingMap;
    bindingMap.reserve(desc.m_Bindings.size());

    uint32_t nextUniformBufferBindingPoint = 1;
    uint32_t nextStorageBufferBindingPoint = 0;
    uint32_t nextTextureUnit = 0;
    uint32_t nextImageUnit = 0;
    std::unordered_map<uint32_t, std::vector<uint32_t>> sampledTextureUnitsBySet;
    std::unordered_map<uint32_t, uint32_t> nextSamplerOrdinalBySet;

    for (const BindingInfo& binding : desc.m_Bindings)
    {
        OpenGLBindingMapEntry entry;
        entry.m_Kind = binding.m_Kind;
        entry.m_SetIndex = binding.m_SetIndex;
        entry.m_LogicalBinding = binding.m_Binding;
        entry.m_Name = binding.m_Name;

        switch (binding.m_Kind)
        {
            case ResourceKind::UniformBuffer:
                entry.m_GlBindingPoint = nextUniformBufferBindingPoint++;
                break;
            case ResourceKind::StorageBuffer:
                entry.m_GlBindingPoint = nextStorageBufferBindingPoint++;
                break;
            case ResourceKind::SampledTexture:
                entry.m_GlTextureUnit = nextTextureUnit++;
                sampledTextureUnitsBySet[entry.m_SetIndex].push_back(entry.m_GlTextureUnit);
                break;
            case ResourceKind::StorageTexture:
                entry.m_GlBindingPoint = nextImageUnit++;
                break;
            case ResourceKind::Sampler:
            {
                const uint32_t samplerOrdinal = nextSamplerOrdinalBySet[entry.m_SetIndex]++;
                const auto sampledTextureUnitsIt = sampledTextureUnitsBySet.find(entry.m_SetIndex);
                if (sampledTextureUnitsIt != sampledTextureUnitsBySet.end() &&
                    samplerOrdinal < sampledTextureUnitsIt->second.size())
                {
                    entry.m_GlTextureUnit = sampledTextureUnitsIt->second[samplerOrdinal];
                }
                else
                {
                    entry.m_GlTextureUnit = nextTextureUnit++;
                }
                break;
            }
        }

        bindingMap.push_back(std::move(entry));
    }

    return bindingMap;
}

void ConfigureOpenGLProgramBindings(GLuint program, const std::vector<OpenGLBindingMapEntry>& bindingMap)
{
    for (const OpenGLBindingMapEntry& entry : bindingMap)
    {
        switch (entry.m_Kind)
        {
            case ResourceKind::UniformBuffer:
            {
                const GLuint blockIndex = glGetUniformBlockIndex(program, entry.m_Name.c_str());
                if (blockIndex != GL_INVALID_INDEX)
                    glUniformBlockBinding(program, blockIndex, entry.m_GlBindingPoint);
                break;
            }
            case ResourceKind::StorageBuffer:
            {
                const GLuint blockIndex =
                    glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, entry.m_Name.c_str());
                if (blockIndex != GL_INVALID_INDEX)
                    glShaderStorageBlockBinding(program, blockIndex, entry.m_GlBindingPoint);
                break;
            }
            case ResourceKind::SampledTexture:
            case ResourceKind::Sampler:
            {
                const GLint location = glGetUniformLocation(program, entry.m_Name.c_str());
                if (location >= 0)
                    glProgramUniform1i(program, location, static_cast<GLint>(entry.m_GlTextureUnit));
                break;
            }
            case ResourceKind::StorageTexture:
            {
                const GLint location = glGetUniformLocation(program, entry.m_Name.c_str());
                if (location >= 0)
                    glProgramUniform1i(program, location, static_cast<GLint>(entry.m_GlBindingPoint));
                break;
            }
        }
    }
}

const OpenGLBindingMapEntry* FindOpenGLBindingMapEntry(const OpenGLGraphicsPipeline& pipeline,
                                                       uint32_t setIndex,
                                                       uint32_t logicalBinding,
                                                       ResourceKind kind)
{
    const auto& bindingMap = pipeline.GetBindingMap();
    const auto it = std::find_if(
        bindingMap.begin(),
        bindingMap.end(),
        [setIndex, logicalBinding, kind](const OpenGLBindingMapEntry& entry)
        { return entry.m_SetIndex == setIndex && entry.m_LogicalBinding == logicalBinding && entry.m_Kind == kind; });
    return it != bindingMap.end() ? &(*it) : nullptr;
}

GLuint GetOpenGLTextureHandle(const TextureBinding& textureBinding)
{
    if (textureBinding.m_View != nullptr)
    {
        const OpenGLTextureView* view = TryGetOpenGLTextureView(textureBinding.m_View);
        RTRLAB_ASSERT_MSG(view != nullptr, "OpenGL resource binding requires an OpenGL TextureView.");
        return view->GetTextureView();
    }

    if (textureBinding.m_Texture != nullptr)
        return GetOpenGLTexture(textureBinding.m_Texture).GetTexture();

    return 0;
}

void ApplyOpenGLResourceSetBinding(const OpenGLGraphicsPipeline& pipeline, ResourceSet* resourceSet)
{
    if (resourceSet == nullptr)
        return;

    OpenGLResourceSet& openGLResourceSet = GetOpenGLResourceSet(resourceSet);
    const PipelineLayout* pipelineLayout = pipeline.GetDesc().m_PipelineLayout;
    RTRLAB_ASSERT_MSG(pipelineLayout != nullptr, "OpenGL resource-set binding requires a graphics pipeline layout.");
    RTRLAB_ASSERT_MSG(resourceSet->GetLayout() == pipelineLayout,
                      "OpenGL resource-set binding requires the ResourceSet to match the bound pipeline layout.");

    const std::vector<const BindingInfo*> setBindings =
        RHIInternal::CollectBindingInfosForSet(pipelineLayout->GetDesc(), resourceSet->GetSetIndex());
    for (const BindingInfo* bindingInfo : setBindings)
    {
        RTRLAB_ASSERT_MSG(bindingInfo != nullptr, "OpenGL binding-map application requires valid binding metadata.");
        const OpenGLBindingMapEntry* bindingMapEntry = FindOpenGLBindingMapEntry(
            pipeline, resourceSet->GetSetIndex(), bindingInfo->m_Binding, bindingInfo->m_Kind);
        RTRLAB_ASSERT_MSG(bindingMapEntry != nullptr, "OpenGL graphics pipeline is missing a binding-map entry.");

        switch (bindingInfo->m_Kind)
        {
            case ResourceKind::UniformBuffer:
            {
                const GLuint constantBuffer =
                    openGLResourceSet.HasConstantBinding() ? openGLResourceSet.GetConstantBuffer() : 0;
                glBindBufferBase(GL_UNIFORM_BUFFER, bindingMapEntry->m_GlBindingPoint, constantBuffer);
                break;
            }
            case ResourceKind::StorageBuffer:
            {
                const BufferBinding* bufferBinding = openGLResourceSet.FindBufferBinding(bindingInfo->m_Binding);
                if (bufferBinding == nullptr || bufferBinding->m_Buffer == nullptr)
                {
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingMapEntry->m_GlBindingPoint, 0);
                    break;
                }

                const OpenGLBuffer& openGLBuffer = GetOpenGLBuffer(bufferBinding->m_Buffer);
                const uint64_t resolvedSize = bufferBinding->m_Size != 0
                                                  ? bufferBinding->m_Size
                                                  : (openGLBuffer.GetDesc().m_Size - bufferBinding->m_Offset);
                glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                  bindingMapEntry->m_GlBindingPoint,
                                  openGLBuffer.GetBuffer(),
                                  static_cast<GLintptr>(bufferBinding->m_Offset),
                                  static_cast<GLsizeiptr>(resolvedSize));
                break;
            }
            case ResourceKind::SampledTexture:
            {
                const TextureBinding* textureBinding = openGLResourceSet.FindTextureBinding(bindingInfo->m_Binding);
                glBindTextureUnit(bindingMapEntry->m_GlTextureUnit,
                                  textureBinding != nullptr ? GetOpenGLTextureHandle(*textureBinding) : 0);
                break;
            }
            case ResourceKind::StorageTexture:
            {
                const TextureBinding* textureBinding = openGLResourceSet.FindTextureBinding(bindingInfo->m_Binding);
                if (textureBinding == nullptr)
                {
                    glBindImageTexture(bindingMapEntry->m_GlBindingPoint, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
                    break;
                }

                GLuint textureHandle = GetOpenGLTextureHandle(*textureBinding);
                Format textureFormat = textureBinding->m_View != nullptr
                                           ? textureBinding->m_View->GetDesc().m_Format
                                           : textureBinding->m_Texture->GetDesc().m_Format;
                if (textureFormat == Format::Unknown && textureBinding->m_Texture != nullptr)
                    textureFormat = textureBinding->m_Texture->GetDesc().m_Format;

                glBindImageTexture(bindingMapEntry->m_GlBindingPoint,
                                   textureHandle,
                                   0,
                                   GL_FALSE,
                                   0,
                                   GL_READ_WRITE,
                                   ToGLInternalFormat(textureFormat));
                break;
            }
            case ResourceKind::Sampler:
            {
                const SamplerBinding* samplerBinding = openGLResourceSet.FindSamplerBinding(bindingInfo->m_Binding);
                const GLuint samplerHandle = samplerBinding != nullptr && samplerBinding->m_Sampler != nullptr
                                                 ? GetOpenGLSampler(samplerBinding->m_Sampler).GetSampler()
                                                 : 0;
                glBindSampler(bindingMapEntry->m_GlTextureUnit, samplerHandle);
                break;
            }
        }
    }
}

GLuint CompileOpenGLShader(GLenum shaderStage, const char* source, GLsizei sourceLength)
{
    RTRLAB_ASSERT_MSG(source != nullptr && sourceLength > 0, "OpenGL shaders require source text.");

    GLuint shader = glCreateShader(shaderStage);
    glShaderSource(shader, 1, &source, &sourceLength);
    glCompileShader(shader);

    GLint compileStatus = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    if (compileStatus != GL_TRUE)
    {
        GLint infoLogLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
        std::string infoLog(static_cast<size_t>(std::max(infoLogLength, 1)), '\0');
        glGetShaderInfoLog(shader, infoLogLength, nullptr, infoLog.data());
        glDeleteShader(shader);
        RTRLAB_ASSERTF(false, "OpenGL shader compilation failed: {}", infoLog);
    }

    return shader;
}
} // namespace

void OpenGLCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    ShellCommandListBase::BeginRendering(renderingInfo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Early clear-only bring-up path:
    // - uses renderArea directly
    // - assumes today's full-window renderArea usage
    // - must translate from the public RHI coordinate convention to GL's lower-left
    //   origin once partial render areas are exercised
    const Rect2D renderArea = renderingInfo.m_RenderArea;
    glViewport(renderArea.m_X,
               renderArea.m_Y,
               static_cast<GLsizei>(renderArea.m_Width),
               static_cast<GLsizei>(renderArea.m_Height));
    glEnable(GL_SCISSOR_TEST);
    glScissor(renderArea.m_X,
              renderArea.m_Y,
              static_cast<GLsizei>(renderArea.m_Width),
              static_cast<GLsizei>(renderArea.m_Height));

    GLbitfield clearMask = 0;

    if (!renderingInfo.m_ColorAttachments.empty())
    {
        // Early bring-up limitation: only the first color attachment clear is consumed here.
        // Extend this to all color attachments when MRT clear support becomes a real requirement.
        const ColorAttachmentInfo& colorAttachment = renderingInfo.m_ColorAttachments.front();
        if (colorAttachment.m_LoadOp == LoadOp::Clear)
        {
            glClearColor(colorAttachment.m_ClearValue.m_R,
                         colorAttachment.m_ClearValue.m_G,
                         colorAttachment.m_ClearValue.m_B,
                         colorAttachment.m_ClearValue.m_A);
            clearMask |= GL_COLOR_BUFFER_BIT;
        }
    }

    if (renderingInfo.m_DepthAttachment.m_View != nullptr && renderingInfo.m_DepthAttachment.m_LoadOp == LoadOp::Clear)
    {
        glClearDepth(renderingInfo.m_DepthAttachment.m_ClearValue.m_Depth);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (clearMask != 0)
        glClear(clearMask);
}

void OpenGLCommandList::EndRendering()
{
    ShellCommandListBase::EndRendering();
    glDisable(GL_SCISSOR_TEST);
}

void OpenGLCommandList::BindGraphicsPipeline(GraphicsPipeline* pipeline)
{
    ShellCommandListBase::BindGraphicsPipeline(pipeline);

    if (pipeline == nullptr)
        return;

    const OpenGLGraphicsPipeline& openGLPipeline = GetOpenGLGraphicsPipeline(pipeline);
    glUseProgram(openGLPipeline.GetProgram());
    glBindVertexArray(openGLPipeline.GetVertexArray());

    for (const auto& resourceSetEntry : m_ResourceSets)
        ApplyOpenGLResourceSetBinding(openGLPipeline, resourceSetEntry.second);
}

void OpenGLCommandList::BindResourceSet(uint32_t setIndex, ResourceSet* resourceSet)
{
    ShellCommandListBase::BindResourceSet(setIndex, resourceSet);

    if (resourceSet == nullptr || m_GraphicsPipeline == nullptr)
        return;

    RTRLAB_ASSERTF(resourceSet->GetSetIndex() == setIndex,
                   "OpenGL BindResourceSet expected resource set {} but received set {}.",
                   setIndex,
                   resourceSet->GetSetIndex());

    const OpenGLGraphicsPipeline& openGLPipeline = GetOpenGLGraphicsPipeline(m_GraphicsPipeline);
    ApplyOpenGLResourceSetBinding(openGLPipeline, resourceSet);
}

void OpenGLCommandList::BindMesh(const MeshBinding& meshBinding, const uint64_t* vertexOffsets)
{
    ShellCommandListBase::BindMesh(meshBinding, vertexOffsets);

    if (!meshBinding.m_VertexBuffers.empty())
    {
        BindVertexBuffers(0,
                          meshBinding.m_VertexBuffers.data(),
                          static_cast<uint32_t>(meshBinding.m_VertexBuffers.size()),
                          vertexOffsets);
    }

    if (meshBinding.m_IndexBuffer != nullptr)
        BindIndexBuffer(meshBinding.m_IndexBuffer, 0, meshBinding.m_IndexType);
}

void OpenGLCommandList::BindVertexBuffers(uint32_t firstSlot,
                                          Buffer* const* buffers,
                                          uint32_t count,
                                          const uint64_t* offsets)
{
    ShellCommandListBase::BindVertexBuffers(firstSlot, buffers, count, offsets);

    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "OpenGL vertex buffers require a bound graphics pipeline.");
    const OpenGLGraphicsPipeline& openGLPipeline = GetOpenGLGraphicsPipeline(m_GraphicsPipeline);
    const VertexInputLayout* vertexInput = openGLPipeline.GetDesc().m_VertexInput;
    RTRLAB_ASSERT_MSG(vertexInput != nullptr, "OpenGL vertex buffers require pipeline vertex input metadata.");

    const auto& bufferLayouts = vertexInput->GetDesc().m_Buffers;
    for (uint32_t index = 0; index < count; ++index)
    {
        const uint32_t bindingIndex = firstSlot + index;
        RTRLAB_ASSERT_MSG(bindingIndex < bufferLayouts.size(), "OpenGL vertex buffer slot exceeds the vertex layout.");
        RTRLAB_ASSERT_MSG(buffers[index] != nullptr, "OpenGL BindVertexBuffers requires non-null buffers.");

        const GLuint buffer = GetOpenGLBuffer(buffers[index]).GetBuffer();
        const GLintptr offset = static_cast<GLintptr>(offsets != nullptr ? offsets[index] : 0);
        const GLsizei stride = static_cast<GLsizei>(bufferLayouts[bindingIndex].m_Stride);
        glVertexArrayVertexBuffer(openGLPipeline.GetVertexArray(), bindingIndex, buffer, offset, stride);
    }
}

void OpenGLCommandList::BindIndexBuffer(Buffer* buffer, uint64_t offset, IndexType indexType)
{
    ShellCommandListBase::BindIndexBuffer(buffer, offset, indexType);

    if (buffer == nullptr)
        return;

    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "OpenGL index buffers require a bound graphics pipeline.");
    const OpenGLGraphicsPipeline& openGLPipeline = GetOpenGLGraphicsPipeline(m_GraphicsPipeline);
    glVertexArrayElementBuffer(openGLPipeline.GetVertexArray(), GetOpenGLBuffer(buffer).GetBuffer());
}

void OpenGLCommandList::SetViewport(float x, float y, float w, float h, float zmin, float zmax)
{
    ShellCommandListBase::SetViewport(x, y, w, h, zmin, zmax);
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(w), static_cast<GLsizei>(h));
    glDepthRange(static_cast<GLdouble>(zmin), static_cast<GLdouble>(zmax));
}

void OpenGLCommandList::SetScissor(int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    ShellCommandListBase::SetScissor(x, y, w, h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
}

void OpenGLCommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset)
{
    ShellCommandListBase::DrawIndexed(indexCount, firstIndex, vertexOffset);

    RTRLAB_ASSERT_MSG(m_IsRendering, "OpenGL DrawIndexed requires an active rendering scope.");
    RTRLAB_ASSERT_MSG(m_GraphicsPipeline != nullptr, "OpenGL DrawIndexed requires a bound graphics pipeline.");
    RTRLAB_ASSERT_MSG(m_IndexBuffer != nullptr, "OpenGL DrawIndexed requires a bound index buffer.");

    const OpenGLGraphicsPipeline& openGLPipeline = GetOpenGLGraphicsPipeline(m_GraphicsPipeline);
    glUseProgram(openGLPipeline.GetProgram());
    glBindVertexArray(openGLPipeline.GetVertexArray());

    const GLenum indexType = ToGLIndexType(m_IndexType);
    const GLsizei indexSize = m_IndexType == IndexType::UInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
    const size_t indexOffsetBytes = static_cast<size_t>(m_IndexOffset + static_cast<uint64_t>(firstIndex) * indexSize);
    glDrawElementsBaseVertex(ToGLPrimitiveTopology(openGLPipeline.GetDesc().m_Topology),
                             static_cast<GLsizei>(indexCount),
                             indexType,
                             reinterpret_cast<const void*>(indexOffsetBytes),
                             vertexOffset);
}

OpenGLSwapchain::OpenGLSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
    : ShellSwapchainBase(desc, nativeWindowHandle)
{
}

void OpenGLSwapchain::Present(uint32_t imageIndex)
{
    ShellSwapchainBase::Present(imageIndex);

    // v1 relies on the current single-window/single-context invariant:
    // the GLFW current context is expected to belong to this swapchain's window.
    // Revisit this path before adding multi-window or multi-context OpenGL support.
    GLFWwindow* currentContext = glfwGetCurrentContext();
    RTRLAB_ASSERT_MSG(currentContext != nullptr, "OpenGLSwapchain::present requires a current GLFW OpenGL context.");
    glfwSwapBuffers(currentContext);
}

Scope<Swapchain> OpenGLDevice::CreateSwapchain(const SwapchainDesc& desc, const NativeWindowHandle& nativeWindowHandle)
{
    return CreateScope<OpenGLSwapchain>(desc, nativeWindowHandle);
}

Scope<Buffer> OpenGLDevice::CreateBuffer(const BufferDesc& desc)
{
    const BufferDesc sanitizedDesc = RHIInternal::SanitizeBufferDesc(desc);
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    // TRANSITIONAL(M4): OpenGL buffer creation still uses mutable storage via
    // glNamedBufferData and maps only MemoryUsage to a coarse usage hint. The
    // target shape is immutable glNamedBufferStorage(..., storageFlags) once the
    // upload model and buffer-usage contract are explicit enough to derive flags.
    glNamedBufferData(
        buffer, static_cast<GLsizeiptr>(sanitizedDesc.m_Size), nullptr, ToGLBufferUsage(sanitizedDesc.m_MemoryUsage));
    SetOpenGLObjectLabel(GL_BUFFER, buffer, sanitizedDesc.m_DebugName);
    return CreateScope<OpenGLBuffer>(buffer, sanitizedDesc);
}

Scope<Texture> OpenGLDevice::CreateTexture(const TextureDesc& desc)
{
    const TextureDesc sanitizedDesc = RHIInternal::SanitizeTextureDesc(desc);
    GLuint texture = 0;
    const GLenum target = ToGLTarget(sanitizedDesc.m_Type);
    const GLenum internalFormat = ToGLInternalFormat(sanitizedDesc.m_Format);

    glCreateTextures(target, 1, &texture);

    switch (sanitizedDesc.m_Type)
    {
        case TextureType::Tex2D:
            glTextureStorage2D(texture,
                               static_cast<GLsizei>(sanitizedDesc.m_MipLevels),
                               internalFormat,
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Width),
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Height));
            break;
        case TextureType::Tex2DArray:
        case TextureType::Cube:
            glTextureStorage3D(texture,
                               static_cast<GLsizei>(sanitizedDesc.m_MipLevels),
                               internalFormat,
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Width),
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Height),
                               static_cast<GLsizei>(sanitizedDesc.m_ArrayLayers));
            break;
        case TextureType::Tex3D:
            glTextureStorage3D(texture,
                               static_cast<GLsizei>(sanitizedDesc.m_MipLevels),
                               internalFormat,
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Width),
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Height),
                               static_cast<GLsizei>(sanitizedDesc.m_Extent.m_Depth));
            break;
    }

    SetOpenGLObjectLabel(GL_TEXTURE, texture, sanitizedDesc.m_DebugName);

    return CreateScope<OpenGLTexture>(texture, target, sanitizedDesc);
}

Scope<TextureView> OpenGLDevice::CreateTextureView(Texture* texture, const TextureViewDesc& desc)
{
    RTRLAB_ASSERT_MSG(texture != nullptr, "OpenGL CreateTextureView requires a valid texture.");

    auto* sourceTexture = dynamic_cast<OpenGLTexture*>(texture);
    RTRLAB_ASSERT_MSG(sourceTexture != nullptr,
                      "OpenGL CreateTextureView only accepts device-created textures. "
                      "Swapchain images expose views via Swapchain::GetImageView().");

    const TextureDesc& sourceDesc = texture->GetDesc();
    const Format viewFormat = desc.m_Format == Format::Unknown ? sourceDesc.m_Format : desc.m_Format;
    const uint32_t mipLevelCount =
        desc.m_MipLevelCount == 0 ? std::max(sourceDesc.m_MipLevels - desc.m_BaseMipLevel, 1u) : desc.m_MipLevelCount;
    const uint32_t arrayLayerCount = desc.m_ArrayLayerCount == 0
                                         ? std::max(sourceDesc.m_ArrayLayers - desc.m_BaseArrayLayer, 1u)
                                         : desc.m_ArrayLayerCount;

    GLuint textureView = 0;
    const GLenum target = ToGLTarget(desc.m_Type);
    glGenTextures(1, &textureView);
    glTextureView(textureView,
                  target,
                  sourceTexture->GetTexture(),
                  ToGLInternalFormat(viewFormat),
                  desc.m_BaseMipLevel,
                  mipLevelCount,
                  desc.m_BaseArrayLayer,
                  arrayLayerCount);
    const std::string debugName = MakeTextureViewDebugName(*texture);
    SetOpenGLObjectLabel(GL_TEXTURE, textureView, debugName.c_str());

    TextureViewDesc resolvedDesc = desc;
    resolvedDesc.m_Format = viewFormat;
    resolvedDesc.m_MipLevelCount = mipLevelCount;
    resolvedDesc.m_ArrayLayerCount = arrayLayerCount;
    return CreateScope<OpenGLTextureView>(texture, textureView, target, resolvedDesc);
}

Scope<Sampler> OpenGLDevice::CreateSampler(const SamplerDesc& desc)
{
    GLuint sampler = 0;
    glCreateSamplers(1, &sampler);
    glSamplerParameteri(
        sampler, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(ToGLMinFilter(desc.m_MinFilter, desc.m_MipFilter)));
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(ToGLFilter(desc.m_MagFilter)));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, static_cast<GLint>(ToGLAddressMode(desc.m_AddressU)));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, static_cast<GLint>(ToGLAddressMode(desc.m_AddressV)));
    glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, static_cast<GLint>(ToGLAddressMode(desc.m_AddressW)));
    // TRANSITIONAL(M4): SamplerDesc does not yet carry border-color or compare-op
    // fields. Shadow-compare samplers and non-default border colors will move
    // through the future shader/reflection-driven sampler contract instead of
    // being hard-coded in this early OpenGL bring-up path.
    glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, desc.m_MinLod);
    glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, desc.m_MaxLod);
    glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS, desc.m_MipLodBias);

    if (desc.m_AnisotropyEnable)
    {
#if defined(GL_TEXTURE_MAX_ANISOTROPY)
        glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, std::max(desc.m_MaxAnisotropy, 1.0f));
#elif defined(GL_TEXTURE_MAX_ANISOTROPY_EXT)
        glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::max(desc.m_MaxAnisotropy, 1.0f));
#endif
    }

    return CreateScope<OpenGLSampler>(sampler, desc);
}

Scope<ShaderProgram> OpenGLDevice::CreateShaderProgram(const CompiledShaderProgramDesc& desc)
{
    std::vector<GLuint> shaders;
    shaders.reserve(desc.m_Blobs.size());

    for (const CompiledShaderBlob& blob : desc.m_Blobs)
    {
        if (blob.m_Backend != BackendType::OpenGL)
            continue;

        RTRLAB_ASSERT_MSG(!blob.m_Code.empty(), "OpenGL shader blobs must contain GLSL source bytes.");
        const char* source = reinterpret_cast<const char*>(blob.m_Code.data());
        const GLsizei sourceLength = static_cast<GLsizei>(blob.m_Code.size());
        shaders.push_back(CompileOpenGLShader(ToGLShaderStage(blob.m_Stage), source, sourceLength));
    }

    RTRLAB_ASSERT_MSG(!shaders.empty(), "OpenGL CreateShaderProgram requires at least one OpenGL shader blob.");

    GLuint program = glCreateProgram();
    for (GLuint shader : shaders)
        glAttachShader(program, shader);
    glLinkProgram(program);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    for (GLuint shader : shaders)
    {
        glDetachShader(program, shader);
        glDeleteShader(shader);
    }

    if (linkStatus != GL_TRUE)
    {
        GLint infoLogLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
        std::string infoLog(static_cast<size_t>(std::max(infoLogLength, 1)), '\0');
        glGetProgramInfoLog(program, infoLogLength, nullptr, infoLog.data());
        glDeleteProgram(program);
        RTRLAB_ASSERTF(false, "OpenGL program link failed: {}", infoLog);
    }

    return CreateScope<OpenGLShaderProgram>(program, desc);
}

Scope<ResourceSet> OpenGLDevice::CreateResourceSet(PipelineLayout* layout, uint32_t setIndex)
{
    return CreateScope<OpenGLResourceSet>(layout, setIndex);
}

Scope<VertexInputLayout> OpenGLDevice::CreateVertexInputLayout(const VertexInputLayoutDesc& desc)
{
    return CreateScope<OpenGLVertexInputLayout>(desc);
}

Scope<GraphicsPipeline> OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    RTRLAB_ASSERT_MSG(desc.m_PipelineLayout != nullptr, "OpenGL graphics pipelines require a PipelineLayout.");
    RTRLAB_ASSERT_MSG(desc.m_ShaderProgram != nullptr, "OpenGL graphics pipelines require a ShaderProgram.");
    RTRLAB_ASSERT_MSG(desc.m_VertexInput != nullptr, "OpenGL graphics pipelines require a VertexInputLayout.");

    const OpenGLShaderProgram& shaderProgram = GetOpenGLShaderProgram(desc.m_ShaderProgram);
    const OpenGLVertexInputLayout& vertexInput = GetOpenGLVertexInputLayout(desc.m_VertexInput);
    std::vector<OpenGLBindingMapEntry> bindingMap = BuildOpenGLBindingMap(desc.m_PipelineLayout->GetDesc());
    ConfigureOpenGLProgramBindings(shaderProgram.GetProgram(), bindingMap);

    GLuint vertexArray = 0;
    glCreateVertexArrays(1, &vertexArray);

    const auto& attributes = vertexInput.GetDesc().m_Attributes;
    for (const VertexAttributeDesc& attribute : attributes)
    {
        RTRLAB_ASSERT_MSG(attribute.m_BufferSlot < vertexInput.GetDesc().m_Buffers.size(),
                          "OpenGL graphics pipelines require valid vertex buffer slots.");

        glEnableVertexArrayAttrib(vertexArray, attribute.m_Location);
        glVertexArrayAttribFormat(vertexArray,
                                  attribute.m_Location,
                                  GetOpenGLAttributeComponentCount(attribute.m_Format),
                                  GL_FLOAT,
                                  GL_FALSE,
                                  attribute.m_Offset);
        glVertexArrayAttribBinding(vertexArray, attribute.m_Location, attribute.m_BufferSlot);
    }

    const auto& bufferLayouts = vertexInput.GetDesc().m_Buffers;
    for (uint32_t bufferIndex = 0; bufferIndex < static_cast<uint32_t>(bufferLayouts.size()); ++bufferIndex)
    {
        glVertexArrayBindingDivisor(vertexArray, bufferIndex, bufferLayouts[bufferIndex].m_PerInstance ? 1u : 0u);
    }

    return CreateScope<OpenGLGraphicsPipeline>(shaderProgram.GetProgram(), vertexArray, desc, std::move(bindingMap));
}

void OpenGLDevice::WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size)
{
    // TRANSITIONAL(M3): Demo-only direct host upload path for early bring-up.
    if (size == 0)
        return;

    RTRLAB_ASSERT_MSG(buffer != nullptr, "OpenGL WriteBuffer requires a valid buffer.");
    RTRLAB_ASSERT_MSG(data != nullptr, "OpenGL WriteBuffer requires non-null source data.");

    OpenGLBuffer& openGLBuffer = GetOpenGLBuffer(buffer);
    RTRLAB_ASSERT_MSG(offset + size <= openGLBuffer.GetDesc().m_Size,
                      "OpenGL WriteBuffer range exceeds the buffer size.");
    glNamedBufferSubData(openGLBuffer.GetBuffer(), static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
}

CommandList* OpenGLDevice::BeginCommandList()
{
    return &m_CommandList;
}

void OpenGLDevice::Submit(CommandList* commandList)
{
    RTRLAB_ASSERT_MSG(commandList == &m_CommandList,
                      "OpenGLDevice::Submit expects the backend-owned command list returned by BeginCommandList().");
    RTRLAB_ASSERT_MSG(!m_CommandList.IsRenderingActive(),
                      "OpenGLDevice::Submit requires EndRendering() before submission.");
    // OpenGL work is recorded directly against the current context as calls are
    // made, so Submit only needs to flush the driver-visible command stream.
    glFlush();
}

FrameContext* OpenGLDevice::BeginFrame()
{
    return &m_FrameContext;
}
