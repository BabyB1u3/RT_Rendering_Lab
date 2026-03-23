#pragma once

/// @file GLGraphicsDevice.h
/// @brief OpenGL implementation of IGraphicsDevice — factory hub for all GL resources.

#include "core/Base.h"
#include "graphics/interface/IGraphicsDevice.h"

class GLRenderCommand;

class GLGraphicsDevice : public IGraphicsDevice
{
public:
	GLGraphicsDevice();
	~GLGraphicsDevice() override = default;

	// --- Buffers ---
	Ref<IVertexBuffer> CreateVertexBuffer(uint32_t size, BufferUsage usage = BufferUsage::DynamicDraw) override;
	Ref<IVertexBuffer> CreateVertexBuffer(const void *data, uint32_t size, BufferUsage usage = BufferUsage::StaticDraw) override;
	Ref<IIndexBuffer> CreateIndexBuffer(const uint32_t *indices, uint32_t count) override;

	// --- Vertex Array ---
	Ref<IVertexArray> CreateVertexArray() override;

	// --- Textures ---
	Ref<ITexture2D> CreateTexture2D(const TextureSpecification &spec) override;
	Ref<ITexture2D> CreateTexture2DFromFile(const std::string &path, bool flipVertically = true) override;

	// --- Shaders ---
	Ref<IShader> CreateShader(const std::string &name) override;
	Ref<IShader> CreateShaderFromSource(const std::string &name, const std::string &vertexSrc, const std::string &fragmentSrc, const std::string &geometrySrc = "") override;
	Ref<IShader> CreateShaderFromFiles(const std::string &name, const std::string &vertexPath, const std::string &fragmentPath, const std::string &geometryPath = "") override;

	// --- Framebuffers ---
	Ref<IFramebuffer> CreateFramebuffer(const FramebufferSpecification &spec) override;

	// --- Render Targets ---
	Ref<IRenderTarget> CreateRenderTargetBackBuffer(uint32_t width, uint32_t height) override;
	Ref<IRenderTarget> CreateRenderTargetFromFramebuffer(const Ref<IFramebuffer> &fb) override;

	// --- Render Commands ---
	Ref<IRenderCommand> GetRenderCommand() override;

private:
	Ref<GLRenderCommand> m_RenderCommand;
};
