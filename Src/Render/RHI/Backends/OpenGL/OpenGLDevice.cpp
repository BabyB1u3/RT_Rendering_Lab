#include "Render/RHI/Backends/OpenGL/OpenGLDevice.h"

#include <algorithm>
#include <string>

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

void SetOpenGLObjectLabel(GLenum identifier, GLuint object, const char* debugName)
{
    if (object == 0 || !HasDebugName(debugName))
        return;

    if (GLAD_GL_VERSION_4_3 || GLAD_GL_KHR_debug)
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
    OpenGLTextureView(Texture* texture, GLuint textureView, GLenum target, bool ownsView, const TextureViewDesc& desc)
        : m_Texture(texture), m_TextureView(textureView), m_Target(target), m_OwnsView(ownsView), m_Desc(desc)
    {
    }

    ~OpenGLTextureView() override
    {
        if (m_OwnsView && m_TextureView != 0)
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
    bool m_OwnsView = false;
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
                               static_cast<GLsizei>(desc.m_Extent.m_Width),
                               static_cast<GLsizei>(desc.m_Extent.m_Height));
            break;
        case TextureType::Tex2DArray:
        case TextureType::Cube:
            glTextureStorage3D(texture,
                               static_cast<GLsizei>(std::max(desc.m_MipLevels, 1u)),
                               internalFormat,
                               static_cast<GLsizei>(desc.m_Extent.m_Width),
                               static_cast<GLsizei>(desc.m_Extent.m_Height),
                               static_cast<GLsizei>(std::max(desc.m_ArrayLayers, 1u)));
            break;
        case TextureType::Tex3D:
            glTextureStorage3D(texture,
                               static_cast<GLsizei>(std::max(desc.m_MipLevels, 1u)),
                               internalFormat,
                               static_cast<GLsizei>(desc.m_Extent.m_Width),
                               static_cast<GLsizei>(desc.m_Extent.m_Height),
                               static_cast<GLsizei>(desc.m_Extent.m_Depth));
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
    return CreateScope<OpenGLTextureView>(texture, textureView, target, true, resolvedDesc);
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

    // Early bring-up limitation: Submit currently validates sequencing only.
    // Real Draw submission will need to honor recorded viewport/scissor state and replay
    // Draw commands instead of relying on BeginRendering()-time clear only.
    (void)m_CommandList.GetRenderingInfo();
}

FrameContext* OpenGLDevice::BeginFrame()
{
    return &m_FrameContext;
}
