#include "MetalRenderCommand.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <unordered_map>

#include "core/Logger.h"
#include "graphics/RenderTypes.h"
#include "graphics/interface/IRenderTarget.h"
#include "graphics/interface/ITexture2D.h"
#include "graphics/metal/MetalCast.h"
#include "graphics/metal/MetalIndexBuffer.h"
#include "graphics/metal/MetalShader.h"
#include "graphics/metal/MetalTexture2D.h"
#include "graphics/metal/MetalTypes.h"
#include "graphics/metal/MetalVertexArray.h"
#include "graphics/metal/MetalVertexBuffer.h"

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct MetalRenderCommand::Impl
{
	id<MTLDevice>              device;
	id<MTLCommandQueue>        commandQueue;
	CAMetalLayer              *layer;

	id<MTLCommandBuffer>          commandBuffer;
	id<MTLRenderCommandEncoder>   encoder;
	id<CAMetalDrawable>           drawable;

	MTLPixelFormat currentColorFormat = MTLPixelFormatBGRA8Unorm;
	MTLPixelFormat currentDepthFormat = MTLPixelFormatInvalid;

	PipelineState currentPipelineState;

	MetalShader      *currentShader = nullptr;
	MetalVertexArray *currentVAO    = nullptr;

	std::unordered_map<uint32_t, Ref<ITexture2D>> boundTextures;

	// Depth-stencil state cache keyed by (depthTest | depthWrite<<1)
	std::unordered_map<uint32_t, id<MTLDepthStencilState>> depthStateCache;
};

// ─── Depth-stencil helpers ────────────────────────────────────────────────────

static id<MTLDepthStencilState> GetOrCreateDepthState(
    id<MTLDevice> device,
    std::unordered_map<uint32_t, id<MTLDepthStencilState>> &cache,
    const PipelineState &ps)
{
	uint32_t key = (ps.DepthTestEnabled  ? 1u : 0u) |
	               (ps.DepthWriteEnabled ? 2u : 0u);

	auto it = cache.find(key);
	if (it != cache.end())
		return it->second;

	MTLDepthStencilDescriptor *desc = [MTLDepthStencilDescriptor new];
	desc.depthWriteEnabled    = ps.DepthWriteEnabled;
	desc.depthCompareFunction = ps.DepthTestEnabled
	    ? MTLCompareFunctionLess
	    : MTLCompareFunctionAlways;

	id<MTLDepthStencilState> state = [device newDepthStencilStateWithDescriptor:desc];
	cache[key] = state;
	return state;
}

// ─── Construction ─────────────────────────────────────────────────────────────

MetalRenderCommand::MetalRenderCommand(void *mtlDevice, void *mtlCommandQueue, void *metalLayer)
	: m_Impl(std::make_unique<Impl>())
{
	m_Impl->device       = (__bridge id<MTLDevice>)mtlDevice;
	m_Impl->commandQueue = (__bridge id<MTLCommandQueue>)mtlCommandQueue;
	m_Impl->layer        = (__bridge CAMetalLayer *)metalLayer;
}

MetalRenderCommand::~MetalRenderCommand() = default;

// ─── IRenderCommand ───────────────────────────────────────────────────────────

void MetalRenderCommand::Init()
{
	// No global state to initialise for Metal.
}

void MetalRenderCommand::BeginFrame()
{
	m_Impl->drawable      = [m_Impl->layer nextDrawable];
	m_Impl->commandBuffer = [m_Impl->commandQueue commandBuffer];
	m_Impl->encoder       = nil;
	m_Impl->currentShader = nullptr;
	m_Impl->currentVAO    = nullptr;
	m_Impl->boundTextures.clear();
}

void MetalRenderCommand::EndFrame()
{
	if (m_Impl->encoder)
	{
		[m_Impl->encoder endEncoding];
		m_Impl->encoder = nil;
	}
	if (m_Impl->drawable)
		[m_Impl->commandBuffer presentDrawable:m_Impl->drawable];

	[m_Impl->commandBuffer commit];
	m_Impl->commandBuffer = nil;
	m_Impl->drawable      = nil;
}

void MetalRenderCommand::BeginRenderPass(const Ref<IRenderTarget> &target,
                                          const RenderPassDescriptor &desc)
{
	// End any open encoder before starting a new one.
	if (m_Impl->encoder)
	{
		[m_Impl->encoder endEncoding];
		m_Impl->encoder = nil;
	}

	MTLRenderPassDescriptor *rpDesc = [MTLRenderPassDescriptor new];

	// ── Color attachment ──────────────────────────────────────────────────────
	id<MTLTexture> colorTex = nil;

	if (target->IsBackBuffer())
	{
		colorTex = m_Impl->drawable ? m_Impl->drawable.texture : nil;
		m_Impl->currentColorFormat = m_Impl->layer.pixelFormat;
	}
	else if (auto colorAtt = target->GetColorAttachment(0))
	{
		colorTex = (__bridge id<MTLTexture>)AsMetal<MetalTexture2D>(colorAtt)->GetMTLTexture();
		m_Impl->currentColorFormat = colorTex ? colorTex.pixelFormat : MTLPixelFormatInvalid;
	}

	if (colorTex)
	{
		rpDesc.colorAttachments[0].texture     = colorTex;
		rpDesc.colorAttachments[0].loadAction  = LoadActionToMTL(desc.ColorLoadAction);
		rpDesc.colorAttachments[0].storeAction = StoreActionToMTL(desc.ColorStoreAction);
		rpDesc.colorAttachments[0].clearColor  = MTLClearColorMake(
		    desc.ClearColor.r, desc.ClearColor.g, desc.ClearColor.b, desc.ClearColor.a);
	}

	// ── Depth attachment ──────────────────────────────────────────────────────
	id<MTLTexture> depthTex = nil;

	if (auto depthAtt = target->GetDepthAttachment())
	{
		depthTex = (__bridge id<MTLTexture>)AsMetal<MetalTexture2D>(depthAtt)->GetMTLTexture();
	}

	if (depthTex)
	{
		m_Impl->currentDepthFormat = depthTex.pixelFormat;
		rpDesc.depthAttachment.texture     = depthTex;
		rpDesc.depthAttachment.loadAction  = LoadActionToMTL(desc.DepthLoadAction);
		rpDesc.depthAttachment.storeAction = StoreActionToMTL(desc.DepthStoreAction);
		rpDesc.depthAttachment.clearDepth  = desc.ClearDepth;

		if (depthTex.pixelFormat == MTLPixelFormatDepth32Float_Stencil8)
		{
			rpDesc.stencilAttachment.texture     = depthTex;
			rpDesc.stencilAttachment.loadAction  = LoadActionToMTL(desc.StencilLoadAction);
			rpDesc.stencilAttachment.storeAction = StoreActionToMTL(desc.StencilStoreAction);
			rpDesc.stencilAttachment.clearStencil = desc.ClearStencil;
		}
	}
	else
	{
		m_Impl->currentDepthFormat = MTLPixelFormatInvalid;
	}

	m_Impl->encoder = [m_Impl->commandBuffer renderCommandEncoderWithDescriptor:rpDesc];
	if (!m_Impl->encoder)
	{
		LOG_ERROR("MetalRenderCommand: failed to create MTLRenderCommandEncoder");
		return;
	}

	// Match OpenGL's default CCW front face / back-face culling convention.
	[m_Impl->encoder setFrontFacingWinding:MTLWindingCounterClockwise];
}

void MetalRenderCommand::EndRenderPass()
{
	if (m_Impl->encoder)
	{
		[m_Impl->encoder endEncoding];
		m_Impl->encoder = nil;
	}
}

void MetalRenderCommand::SetPipelineState(const PipelineState &state)
{
	m_Impl->currentPipelineState = state;

	if (!m_Impl->encoder)
		return;

	// Cull mode is per-encoder in Metal (not baked into the PSO).
	if (state.CullFaceEnabled)
		[m_Impl->encoder setCullMode:state.CullFront ? MTLCullModeFront : MTLCullModeBack];
	else
		[m_Impl->encoder setCullMode:MTLCullModeNone];
}

void MetalRenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	if (!m_Impl->encoder)
		return;

	MTLViewport vp;
	vp.originX = static_cast<double>(x);
	vp.originY = static_cast<double>(y);
	vp.width   = static_cast<double>(width);
	vp.height  = static_cast<double>(height);
	vp.znear   = 0.0;
	vp.zfar    = 1.0;
	[m_Impl->encoder setViewport:vp];
}

void MetalRenderCommand::SetTexture(uint32_t slot, const Ref<ITexture2D> &texture)
{
	if (texture)
		m_Impl->boundTextures[slot] = texture;
	else
		m_Impl->boundTextures.erase(slot);
}

void MetalRenderCommand::DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount)
{
	if (!m_Impl->encoder || !m_Impl->currentShader)
	{
		LOG_WARN("MetalRenderCommand::DrawIndexed: no active encoder or shader");
		return;
	}

	MetalVertexArray *metalVAO = m_Impl->currentVAO;
	if (vao)
	{
		metalVAO = AsMetal<MetalVertexArray>(vao);
		m_Impl->currentVAO = metalVAO;
	}

	if (!metalVAO)
	{
		LOG_WARN("MetalRenderCommand::DrawIndexed: no active vertex array");
		return;
	}

	// ── Pipeline state object ─────────────────────────────────────────────────
	void *pso = m_Impl->currentShader->GetOrCreatePSO(
	    (__bridge void *)m_Impl->device,
	    metalVAO->GetMTLVertexDescriptor(),
	    static_cast<uint32_t>(m_Impl->currentColorFormat),
	    static_cast<uint32_t>(m_Impl->currentDepthFormat),
	    m_Impl->currentPipelineState);

	if (!pso) return;
	[m_Impl->encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)pso];

	// ── Depth-stencil state ───────────────────────────────────────────────────
	id<MTLDepthStencilState> ds = GetOrCreateDepthState(
	    m_Impl->device, m_Impl->depthStateCache, m_Impl->currentPipelineState);
	[m_Impl->encoder setDepthStencilState:ds];

	// ── Vertex buffers ────────────────────────────────────────────────────────
	const auto &vbs = metalVAO->GetVertexBuffers();
	for (uint32_t slot = 0; slot < static_cast<uint32_t>(vbs.size()); ++slot)
	{
		auto *vb = AsMetal<MetalVertexBuffer>(vbs[slot]);
		[m_Impl->encoder setVertexBuffer:(__bridge id<MTLBuffer>)vb->GetMTLBuffer()
		                          offset:vb->GetCurrentOffset()
		                         atIndex:slot];
	}

	// ── Uniforms ──────────────────────────────────────────────────────────────
	m_Impl->currentShader->FlushUniforms((__bridge void *)m_Impl->encoder);

	// ── Textures ──────────────────────────────────────────────────────────────
	for (const auto &[slot, tex] : m_Impl->boundTextures)
	{
		auto *metalTex = AsMetal<MetalTexture2D>(tex);
		[m_Impl->encoder setFragmentTexture:(__bridge id<MTLTexture>)metalTex->GetMTLTexture()
		                            atIndex:slot];
		[m_Impl->encoder setFragmentSamplerState:(__bridge id<MTLSamplerState>)metalTex->GetMTLSamplerState()
		                                 atIndex:slot];
	}

	// ── Draw call ─────────────────────────────────────────────────────────────
	auto *ib   = AsMetal<MetalIndexBuffer>(metalVAO->GetIndexBuffer());
	uint32_t count = indexCount ? indexCount : ib->GetCount();

	[m_Impl->encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
	                            indexCount:count
	                             indexType:MTLIndexTypeUInt32
	                           indexBuffer:(__bridge id<MTLBuffer>)ib->GetMTLBuffer()
	                     indexBufferOffset:0];
}

void MetalRenderCommand::DrawArrays(uint32_t /*mode*/, uint32_t first, uint32_t count)
{
	if (!m_Impl->encoder || !m_Impl->currentShader)
	{
		LOG_WARN("MetalRenderCommand::DrawArrays: no active encoder or shader");
		return;
	}

	void *vertexDescriptor = nullptr;
	if (m_Impl->currentVAO)
		vertexDescriptor = m_Impl->currentVAO->GetMTLVertexDescriptor();

	// Pipeline state + depth state
	void *pso = m_Impl->currentShader->GetOrCreatePSO(
	    (__bridge void *)m_Impl->device,
	    vertexDescriptor,
	    static_cast<uint32_t>(m_Impl->currentColorFormat),
	    static_cast<uint32_t>(m_Impl->currentDepthFormat),
	    m_Impl->currentPipelineState);

	if (!pso) return;
	[m_Impl->encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)pso];

	id<MTLDepthStencilState> ds = GetOrCreateDepthState(
	    m_Impl->device, m_Impl->depthStateCache, m_Impl->currentPipelineState);
	[m_Impl->encoder setDepthStencilState:ds];

	if (m_Impl->currentVAO)
	{
		const auto &vbs = m_Impl->currentVAO->GetVertexBuffers();
		for (uint32_t slot = 0; slot < static_cast<uint32_t>(vbs.size()); ++slot)
		{
			auto *vb = AsMetal<MetalVertexBuffer>(vbs[slot]);
			[m_Impl->encoder setVertexBuffer:(__bridge id<MTLBuffer>)vb->GetMTLBuffer()
			                          offset:vb->GetCurrentOffset()
			                         atIndex:slot];
		}
	}

	m_Impl->currentShader->FlushUniforms((__bridge void *)m_Impl->encoder);

	for (const auto &[slot, tex] : m_Impl->boundTextures)
	{
		auto *metalTex = AsMetal<MetalTexture2D>(tex);
		[m_Impl->encoder setFragmentTexture:(__bridge id<MTLTexture>)metalTex->GetMTLTexture()
		                            atIndex:slot];
		[m_Impl->encoder setFragmentSamplerState:(__bridge id<MTLSamplerState>)metalTex->GetMTLSamplerState()
		                                 atIndex:slot];
	}

	[m_Impl->encoder drawPrimitives:MTLPrimitiveTypeTriangle
	                    vertexStart:first
	                    vertexCount:count];
}

// ─── Metal-internal ───────────────────────────────────────────────────────────

void MetalRenderCommand::SetCurrentShader(MetalShader *shader)
{
	m_Impl->currentShader = shader;
}

void MetalRenderCommand::SetCurrentVAO(MetalVertexArray *vao)
{
	m_Impl->currentVAO = vao;
}
