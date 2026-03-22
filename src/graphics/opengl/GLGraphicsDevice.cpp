#include "GLGraphicsDevice.h"

#include "GLTexture2D.h"
#include "GLVertexBuffer.h"
#include "GLIndexBuffer.h"
#include "GLVertexArray.h"
#include "GLShader.h"
#include "GLFramebuffer.h"
#include "GLRenderTarget.h"
#include "GLRenderCommand.h"

GLGraphicsDevice::GLGraphicsDevice()
	: m_RenderCommand(CreateRef<GLRenderCommand>())
{
}

// --- Buffers ---

Ref<IVertexBuffer> GLGraphicsDevice::CreateVertexBuffer(uint32_t size, BufferUsage usage)
{
	return CreateRef<GLVertexBuffer>(size, usage);
}

Ref<IVertexBuffer> GLGraphicsDevice::CreateVertexBuffer(const void *data, uint32_t size, BufferUsage usage)
{
	return CreateRef<GLVertexBuffer>(data, size, usage);
}

Ref<IIndexBuffer> GLGraphicsDevice::CreateIndexBuffer(const uint32_t *indices, uint32_t count)
{
	return CreateRef<GLIndexBuffer>(indices, count);
}

// --- Vertex Array ---

Ref<IVertexArray> GLGraphicsDevice::CreateVertexArray()
{
	return CreateRef<GLVertexArray>();
}

// --- Textures ---

Ref<ITexture2D> GLGraphicsDevice::CreateTexture2D(const TextureSpecification &spec)
{
	return GLTexture2D::Create(spec);
}

Ref<ITexture2D> GLGraphicsDevice::CreateTexture2DFromFile(const std::string &path, bool flipVertically)
{
	return GLTexture2D::CreateFromFile(path, flipVertically);
}

// --- Shaders ---

Ref<IShader> GLGraphicsDevice::CreateShaderFromSource(const std::string &name, const std::string &vertexSrc, const std::string &fragmentSrc, const std::string &geometrySrc)
{
	return GLShader::CreateFromSource(name, vertexSrc, fragmentSrc, geometrySrc);
}

Ref<IShader> GLGraphicsDevice::CreateShaderFromFiles(const std::string &name, const std::string &vertexPath, const std::string &fragmentPath, const std::string &geometryPath)
{
	return GLShader::CreateFromFiles(name, vertexPath, fragmentPath, geometryPath);
}

Ref<IShader> GLGraphicsDevice::CreateShaderFromSingleFile(const std::string &filepath, const std::string &name)
{
	return GLShader::CreateFromSingleFile(filepath, name);
}

// --- Framebuffers ---

Ref<IFramebuffer> GLGraphicsDevice::CreateFramebuffer(const FramebufferSpecification &spec)
{
	return CreateRef<GLFramebuffer>(spec);
}

// --- Render Targets ---

Ref<IRenderTarget> GLGraphicsDevice::CreateRenderTargetBackBuffer(uint32_t width, uint32_t height)
{
	return GLRenderTarget::CreateBackBuffer(width, height);
}

Ref<IRenderTarget> GLGraphicsDevice::CreateRenderTargetFromFramebuffer(const Ref<IFramebuffer> &fb)
{
	return GLRenderTarget::CreateFromFramebuffer(fb);
}

// --- Render Commands ---

Ref<IRenderCommand> GLGraphicsDevice::GetRenderCommand()
{
	return m_RenderCommand;
}
