#pragma once

/// @file IGraphicsDevice.h
/// @brief Abstract factory for creating backend-specific graphics resources.
///
/// Replaces all static Create/CreateFromFile methods on concrete classes.
/// A global accessor (e.g. GetDevice()) is set once at Application init
/// based on platform/config.

#include <cstdint>
#include <string>

#include "core/Base.h"
#include "graphics/Buffers.h" // BufferUsage

class IVertexBuffer;
class IIndexBuffer;
class IVertexArray;
class ITexture2D;
class IShader;
class IFramebuffer;
class IRenderTarget;
class IRenderCommand;
struct TextureSpecification;
struct FramebufferSpecification;

class IGraphicsDevice
{
public:
	virtual ~IGraphicsDevice() = default;

	// --- Buffers ---
	virtual Ref<IVertexBuffer> CreateVertexBuffer(uint32_t size, BufferUsage usage = BufferUsage::DynamicDraw) = 0;
	virtual Ref<IVertexBuffer> CreateVertexBuffer(const void *data, uint32_t size, BufferUsage usage = BufferUsage::StaticDraw) = 0;
	virtual Ref<IIndexBuffer> CreateIndexBuffer(const uint32_t *indices, uint32_t count) = 0;

	// --- Vertex Array ---
	virtual Ref<IVertexArray> CreateVertexArray() = 0;

	// --- Textures ---
	virtual Ref<ITexture2D> CreateTexture2D(const TextureSpecification &spec) = 0;
	virtual Ref<ITexture2D> CreateTexture2DFromFile(const std::string &path, bool flipVertically = true) = 0;

	// --- Shaders ---
	virtual Ref<IShader> CreateShaderFromSource(const std::string &name, const std::string &vertexSrc, const std::string &fragmentSrc, const std::string &geometrySrc = "") = 0;
	virtual Ref<IShader> CreateShaderFromFiles(const std::string &name, const std::string &vertexPath, const std::string &fragmentPath, const std::string &geometryPath = "") = 0;
	virtual Ref<IShader> CreateShaderFromSingleFile(const std::string &filepath, const std::string &name = "") = 0;

	// --- Framebuffers ---
	virtual Ref<IFramebuffer> CreateFramebuffer(const FramebufferSpecification &spec) = 0;

	// --- Render Targets ---
	virtual Ref<IRenderTarget> CreateRenderTargetBackBuffer(uint32_t width, uint32_t height) = 0;
	virtual Ref<IRenderTarget> CreateRenderTargetFromFramebuffer(const Ref<IFramebuffer> &fb) = 0;

	// --- Render Commands ---
	virtual Ref<IRenderCommand> GetRenderCommand() = 0;
};
