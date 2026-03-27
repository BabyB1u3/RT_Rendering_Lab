#pragma once

/// @file MetalGraphicsDevice.h
/// @brief Metal implementation of IGraphicsDevice.
///
/// Uses pImpl to keep all Objective-C types out of this header.
/// Plain C++ - no Metal imports leak into the rest of the codebase.

#include <memory>
#include <string>
#include <cstdint>

#include "core/Base.h"
#include "graphics/interfaces/IGraphicsDevice.h"

struct GLFWwindow;
class MetalRenderCommand;

class MetalGraphicsDevice : public IGraphicsDevice
{
public:
	/// Sets up MTLDevice, MTLCommandQueue, and CAMetalLayer from the GLFW window.
	explicit MetalGraphicsDevice(GLFWwindow *window);
	~MetalGraphicsDevice() override;

	MetalGraphicsDevice(const MetalGraphicsDevice &) = delete;
	MetalGraphicsDevice &operator=(const MetalGraphicsDevice &) = delete;

	// --- IGraphicsDevice ---
	Ref<IVertexBuffer> CreateVertexBuffer(uint32_t size, BufferUsage usage) override;
	Ref<IVertexBuffer> CreateVertexBuffer(const void *data, uint32_t size, BufferUsage usage) override;
	Ref<IIndexBuffer> CreateIndexBuffer(const uint32_t *indices, uint32_t count) override;
	Ref<IVertexArray> CreateVertexArray() override;
	Ref<ITexture2D> CreateTexture2D(const TextureSpecification &spec) override;
	Ref<ITexture2D> CreateTexture2DFromFile(const std::string &path, bool flipVertically) override;
	Ref<IShader> CreateShader(const std::string &name) override;
	Ref<IShader> CreateShaderFromSource(const std::string &name, const std::string &vertexSrc,
										const std::string &fragmentSrc,
										const std::string &geometrySrc) override;
	Ref<IShader> CreateShaderFromFiles(const std::string &name, const std::string &vertexPath,
									   const std::string &fragmentPath,
									   const std::string &geometryPath) override;
	Ref<IFramebuffer> CreateFramebuffer(const FramebufferSpecification &spec) override;
	Ref<IRenderTarget> CreateRenderTargetBackBuffer(uint32_t width, uint32_t height) override;
	Ref<IRenderTarget> CreateRenderTargetFromFramebuffer(const Ref<IFramebuffer> &fb) override;
	Ref<IRenderCommand> GetRenderCommand() override;
	void OnResize(uint32_t width, uint32_t height) override;

	// --- Metal-internal (call only from .mm files) ---
	/// Returns id<MTLDevice> as void*.
	void *GetMTLDevice() const;
	/// Returns id<MTLCommandQueue> as void*.
	void *GetMTLCommandQueue() const;
	/// Returns CAMetalLayer* as void*.
	void *GetMetalLayer() const;
	/// Direct access to the concrete render command for intra-backend communication.
	MetalRenderCommand *GetMetalRenderCommand() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};
