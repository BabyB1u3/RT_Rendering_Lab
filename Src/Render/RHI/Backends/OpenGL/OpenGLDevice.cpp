#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

#include <algorithm>
#include <cstring>
#include <string>
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
    OpenGLGraphicsPipeline(GLuint program, GLuint vertexArray, const GraphicsPipelineDesc& desc)
        : m_Program(program), m_VertexArray(vertexArray), m_Desc(desc)
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

private:
    GLuint m_Program = 0;
    GLuint m_VertexArray = 0;
    GraphicsPipelineDesc m_Desc;
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

GLuint CompileOpenGLShader(GLenum shaderStage, const char* source)
{
    RTRLAB_ASSERT_MSG(source != nullptr && source[0] != '\0', "OpenGL shaders require source text.");

    GLuint shader = glCreateShader(shaderStage);
    glShaderSource(shader, 1, &source, nullptr);
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
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    glNamedBufferData(buffer,
                      static_cast<GLsizeiptr>(std::max<uint64_t>(desc.m_Size, 1)),
                      nullptr,
                      ToGLBufferUsage(desc.m_MemoryUsage));
    SetOpenGLObjectLabel(GL_BUFFER, buffer, desc.m_DebugName);
    return CreateScope<OpenGLBuffer>(buffer, desc);
}

Scope<Texture> OpenGLDevice::CreateTexture(const TextureDesc& desc)
{
    GLuint texture = 0;
    const GLenum target = ToGLTarget(desc.m_Type);
    const GLenum internalFormat = ToGLInternalFormat(desc.m_Format);

    glCreateTextures(target, 1, &texture);

    switch (desc.m_Type)
    {
        case TextureType::Tex2D:
            glTextureStorage2D(texture,
                               static_cast<GLsizei>(std::max(desc.m_MipLevels, 1u)),
                               internalFormat,
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Width, 1u)),
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Height, 1u)));
            break;
        case TextureType::Tex2DArray:
        case TextureType::Cube:
            glTextureStorage3D(texture,
                               static_cast<GLsizei>(std::max(desc.m_MipLevels, 1u)),
                               internalFormat,
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Width, 1u)),
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Height, 1u)),
                               static_cast<GLsizei>(std::max(desc.m_ArrayLayers, 1u)));
            break;
        case TextureType::Tex3D:
            glTextureStorage3D(texture,
                               static_cast<GLsizei>(std::max(desc.m_MipLevels, 1u)),
                               internalFormat,
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Width, 1u)),
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Height, 1u)),
                               static_cast<GLsizei>(std::max(desc.m_Extent.m_Depth, 1u)));
            break;
    }

    SetOpenGLObjectLabel(GL_TEXTURE, texture, desc.m_DebugName);

    return CreateScope<OpenGLTexture>(texture, target, desc);
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
        shaders.push_back(CompileOpenGLShader(ToGLShaderStage(blob.m_Stage), source));
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

Scope<VertexInputLayout> OpenGLDevice::CreateVertexInputLayout(const VertexInputLayoutDesc& desc)
{
    return CreateScope<OpenGLVertexInputLayout>(desc);
}

Scope<GraphicsPipeline> OpenGLDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
{
    RTRLAB_ASSERT_MSG(desc.m_ShaderProgram != nullptr, "OpenGL graphics pipelines require a ShaderProgram.");
    RTRLAB_ASSERT_MSG(desc.m_VertexInput != nullptr, "OpenGL graphics pipelines require a VertexInputLayout.");

    const OpenGLShaderProgram& shaderProgram = GetOpenGLShaderProgram(desc.m_ShaderProgram);
    const OpenGLVertexInputLayout& vertexInput = GetOpenGLVertexInputLayout(desc.m_VertexInput);

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

    return CreateScope<OpenGLGraphicsPipeline>(shaderProgram.GetProgram(), vertexArray, desc);
}

void OpenGLDevice::WriteBuffer(Buffer* buffer, uint64_t offset, const void* data, uint64_t size)
{
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
}

FrameContext* OpenGLDevice::BeginFrame()
{
    return &m_FrameContext;
}
