#include "MetalGraphicsDevice.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>

#include <stdexcept>

#include "core/Logger.h"
#include "graphics/metal/MetalFramebuffer.h"
#include "graphics/metal/MetalGraphicsDevice.h"
#include "graphics/metal/MetalIndexBuffer.h"
#include "graphics/metal/MetalRenderCommand.h"
#include "graphics/metal/MetalRenderTarget.h"
#include "graphics/metal/MetalShader.h"
#include "graphics/metal/MetalTexture2D.h"
#include "graphics/metal/MetalVertexArray.h"
#include "graphics/metal/MetalVertexBuffer.h"

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct MetalGraphicsDevice::Impl
{
	id<MTLDevice>       device;
	id<MTLCommandQueue> commandQueue;
	CAMetalLayer       *layer;

	Ref<MetalRenderCommand> renderCommand;
};

// ─── Construction ─────────────────────────────────────────────────────────────

MetalGraphicsDevice::MetalGraphicsDevice(GLFWwindow *window)
	: m_Impl(std::make_unique<Impl>())
{
	m_Impl->device = MTLCreateSystemDefaultDevice();
	if (!m_Impl->device)
		throw std::runtime_error("MetalGraphicsDevice: MTLCreateSystemDefaultDevice() returned nil");

	m_Impl->commandQueue = [m_Impl->device newCommandQueue];

	// ── Set up CAMetalLayer on the GLFW window's NSView ───────────────────────
	NSWindow *nswindow  = glfwGetCocoaWindow(window);
	NSView   *view      = nswindow.contentView;

	m_Impl->layer             = [CAMetalLayer layer];
	m_Impl->layer.device      = m_Impl->device;
	m_Impl->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	m_Impl->layer.framebufferOnly = YES;

	// Match the view's drawable size to the backing resolution (Retina support).
	CGSize size = view.bounds.size;
	size.width  *= nswindow.backingScaleFactor;
	size.height *= nswindow.backingScaleFactor;
	m_Impl->layer.drawableSize = size;

	view.layer       = m_Impl->layer;
	view.wantsLayer  = YES;

	// ── Create render command ─────────────────────────────────────────────────
	m_Impl->renderCommand = CreateRef<MetalRenderCommand>(
	    (__bridge void *)m_Impl->device,
	    (__bridge void *)m_Impl->commandQueue,
	    (__bridge void *)m_Impl->layer);

	LOG_INFO("MetalGraphicsDevice: {}", [m_Impl->device.name UTF8String]);
}

MetalGraphicsDevice::~MetalGraphicsDevice() = default;

// ─── IGraphicsDevice — Buffers ────────────────────────────────────────────────

Ref<IVertexBuffer> MetalGraphicsDevice::CreateVertexBuffer(uint32_t size, BufferUsage usage)
{
	return CreateRef<MetalVertexBuffer>(size, usage);
}

Ref<IVertexBuffer> MetalGraphicsDevice::CreateVertexBuffer(const void *data, uint32_t size,
                                                            BufferUsage usage)
{
	return CreateRef<MetalVertexBuffer>(data, size, usage);
}

Ref<IIndexBuffer> MetalGraphicsDevice::CreateIndexBuffer(const uint32_t *indices, uint32_t count)
{
	return CreateRef<MetalIndexBuffer>(indices, count);
}

// ─── IGraphicsDevice — Vertex Array ──────────────────────────────────────────

Ref<IVertexArray> MetalGraphicsDevice::CreateVertexArray()
{
	return CreateRef<MetalVertexArray>();
}

// ─── IGraphicsDevice — Textures ──────────────────────────────────────────────

Ref<ITexture2D> MetalGraphicsDevice::CreateTexture2D(const TextureSpecification &spec)
{
	return MetalTexture2D::Create(spec);
}

Ref<ITexture2D> MetalGraphicsDevice::CreateTexture2DFromFile(const std::string &path,
                                                               bool flipVertically)
{
	return MetalTexture2D::CreateFromFile(path, flipVertically);
}

// ─── IGraphicsDevice — Shaders ───────────────────────────────────────────────

Ref<IShader> MetalGraphicsDevice::CreateShader(const std::string &name)
{
	return MetalShader::CreateFromCompiledMSL(name);
}

Ref<IShader> MetalGraphicsDevice::CreateShaderFromSource(const std::string &name,
                                                           const std::string &vertexSrc,
                                                           const std::string &fragmentSrc,
                                                           const std::string & /*geometrySrc*/)
{
	// Metal uses a single MSL source; vertexSrc is treated as the full MSL source.
	// geometrySrc is unsupported in Metal and ignored.
	return MetalShader::CreateFromMSLSource(name, vertexSrc.empty() ? fragmentSrc : vertexSrc);
}

Ref<IShader> MetalGraphicsDevice::CreateShaderFromFiles(const std::string &name,
                                                          const std::string &vertexPath,
                                                          const std::string &fragmentPath,
                                                          const std::string & /*geometryPath*/)
{
	// For Metal, treat the vertex path as the combined MSL file.
	return MetalShader::CreateFromCompiledMSL(name);
	(void)vertexPath; (void)fragmentPath;
}

// ─── IGraphicsDevice — Framebuffers ──────────────────────────────────────────

Ref<IFramebuffer> MetalGraphicsDevice::CreateFramebuffer(const FramebufferSpecification &spec)
{
	return CreateRef<MetalFramebuffer>(spec);
}

// ─── IGraphicsDevice — Render Targets ────────────────────────────────────────

Ref<IRenderTarget> MetalGraphicsDevice::CreateRenderTargetBackBuffer(uint32_t width,
                                                                      uint32_t height)
{
	return MetalRenderTarget::CreateBackBuffer(width, height);
}

Ref<IRenderTarget> MetalGraphicsDevice::CreateRenderTargetFromFramebuffer(
    const Ref<IFramebuffer> &fb)
{
	return MetalRenderTarget::CreateFromFramebuffer(fb);
}

// ─── IGraphicsDevice — Render Commands ───────────────────────────────────────

Ref<IRenderCommand> MetalGraphicsDevice::GetRenderCommand()
{
	return m_Impl->renderCommand;
}

void MetalGraphicsDevice::OnResize(uint32_t width, uint32_t height)
{
	m_Impl->layer.drawableSize = CGSizeMake(static_cast<CGFloat>(width),
	                                        static_cast<CGFloat>(height));
}

// ─── Metal-internal ───────────────────────────────────────────────────────────

void *MetalGraphicsDevice::GetMTLDevice() const
{
	return (__bridge void *)m_Impl->device;
}

void *MetalGraphicsDevice::GetMTLCommandQueue() const
{
	return (__bridge void *)m_Impl->commandQueue;
}

void *MetalGraphicsDevice::GetMetalLayer() const
{
	return (__bridge void *)m_Impl->layer;
}

MetalRenderCommand *MetalGraphicsDevice::GetMetalRenderCommand() const
{
	return m_Impl->renderCommand.get();
}
