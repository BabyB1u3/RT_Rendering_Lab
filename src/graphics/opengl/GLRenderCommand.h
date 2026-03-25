#pragma once

/// @file GLRenderCommand.h
/// @brief OpenGL implementation of IRenderCommand.
///
/// Translates the explicit render pass / pipeline state model back to
/// OpenGL's immediate-mode state machine.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/interface/IRenderCommand.h"

class GLRenderCommand : public IRenderCommand
{
public:
	GLRenderCommand() = default;
	~GLRenderCommand() override = default;

	void Init() override;

	// --- Frame lifecycle (no-op for OpenGL) ---
	void BeginFrame() override {}
	void EndFrame() override {}

	// --- Render pass ---
	void BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc) override;
	void EndRenderPass() override;

	// --- Pipeline state ---
	void SetPipelineState(const PipelineState &state) override;

	void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

	// --- Resource binding ---
	void SetTexture(uint32_t slot, const Ref<ITexture2D> &texture) override;

	void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0) override;
	void DrawArrays(uint32_t mode, uint32_t first, uint32_t count) override;
};
