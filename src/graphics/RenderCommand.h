#pragma once

/// @file RenderCommand.h
/// @brief Static forwarding shim that delegates to the active graphics device.
///
/// All methods are static - RenderCommand forwards each call to
/// GetDevice()->GetRenderCommand(). Render passes call these instead of
/// interacting with the backend directly.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/RenderTypes.h"

class IRenderTarget;
class ITexture2D;
class IVertexArray;

class RenderCommand
{
public:
	/// One-time initialization (e.g. default GL state).
	static void Init();

	// --- Frame lifecycle ---
	static void BeginFrame();
	static void EndFrame();

	// --- Render pass ---
	static void BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc);
	static void EndRenderPass();

	// --- Pipeline state ---
	static void SetPipelineState(const PipelineState &state);

	static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

	// --- Resource binding ---
	static void SetTexture(uint32_t slot, const Ref<ITexture2D> &texture);

	/// Draw indexed triangles. If indexCount is 0, uses the full index buffer.
	static void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0);
	static void DrawArrays(uint32_t mode, uint32_t first, uint32_t count);
};
