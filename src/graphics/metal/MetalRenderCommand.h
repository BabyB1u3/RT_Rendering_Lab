#pragma once

/// @file MetalRenderCommand.h
/// @brief Metal implementation of IRenderCommand.
///
/// Manages the per-frame MTLCommandBuffer / MTLRenderCommandEncoder lifecycle.
/// BeginFrame acquires a drawable and allocates a command buffer; each
/// BeginRenderPass creates a new encoder; EndFrame commits and presents.
///
/// Uses pImpl — no Metal types are visible in this header.

#include <cstdint>
#include <memory>

#include "core/Base.h"
#include "graphics/interface/IRenderCommand.h"

class MetalShader;
class MetalVertexArray;

class MetalRenderCommand : public IRenderCommand
{
public:
	/// @param mtlDevice        id<MTLDevice>       as void*
	/// @param mtlCommandQueue  id<MTLCommandQueue> as void*
	/// @param metalLayer       CAMetalLayer*       as void*
	MetalRenderCommand(void *mtlDevice, void *mtlCommandQueue, void *metalLayer);
	~MetalRenderCommand() override;

	MetalRenderCommand(const MetalRenderCommand &) = delete;
	MetalRenderCommand &operator=(const MetalRenderCommand &) = delete;

	// --- IRenderCommand ---
	void Init() override;

	void BeginFrame() override;
	void EndFrame() override;

	void BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc) override;
	void EndRenderPass() override;

	void SetPipelineState(const PipelineState &state) override;
	void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
	void SetTexture(uint32_t slot, const Ref<ITexture2D> &texture) override;

	void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0) override;
	void DrawArrays(uint32_t mode, uint32_t first, uint32_t count) override;

	// --- Metal-internal (called by MetalShader::Bind and MetalVertexArray::Bind) ---
	void SetCurrentShader(MetalShader *shader);
	void SetCurrentVAO(MetalVertexArray *vao);

private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};
