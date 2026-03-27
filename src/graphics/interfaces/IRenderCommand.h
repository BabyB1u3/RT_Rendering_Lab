#pragma once

/// @file IRenderCommand.h
/// @brief Abstract interface for render state and draw commands. Backend-agnostic.
///
/// Render passes interact with the GPU exclusively through this interface.
/// The command model is designed for explicit APIs (Metal, Vulkan):
///
///   BeginFrame()
///     BeginRenderPass(target, descriptor)  - one per render target switch
///       SetPipelineState(state)
///       SetViewport(...)
///       SetTexture(slot, texture)
///       shader->SetUniformBlock(...)
///       DrawIndexed(...) / DrawArrays(...)
///     EndRenderPass()
///   EndFrame()
///
/// OpenGL implements these by translating to its immediate-mode state machine.
/// Metal/Vulkan map them directly to command encoders / command buffers.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"
#include "graphics/RenderTypes.h"

class IRenderTarget;
class ITexture2D;
class IVertexArray;

class IRenderCommand
{
public:
	virtual ~IRenderCommand() = default;

	/// One-time initialization (e.g. default GL state).
	virtual void Init() = 0;

	// --- Frame lifecycle (P1) ---
	// Metal: create MTLCommandBuffer, acquire CAMetalDrawable.
	// Vulkan: begin command buffer recording.
	// OpenGL: no-op (immediate mode has no frame concept).
	virtual void BeginFrame() {}
	virtual void EndFrame() {}

	// --- Render pass (P2) ---
	// Replaces target->Bind() + SetClearColor() + Clear().
	// Metal: create MTLRenderCommandEncoder with MTLRenderPassDescriptor.
	// Vulkan: vkCmdBeginRenderPass with VkRenderPassBeginInfo.
	// OpenGL: bind FBO + glClearColor + glClear.
	virtual void BeginRenderPass(const Ref<IRenderTarget> &target, const RenderPassDescriptor &desc) = 0;
	virtual void EndRenderPass() = 0;

	// --- Pipeline state (P3) ---
	// Replaces EnableDepthTest(), EnableBlend(), EnableCullFace(), SetCullFace().
	// Metal/Vulkan: look up or create a cached pipeline state object.
	// OpenGL: set individual glEnable/glDisable flags.
	virtual void SetPipelineState(const PipelineState &state) = 0;

	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

	// --- Resource binding (P4) ---
	// Replaces texture->Bind(slot) calls scattered through render pass code.
	// Metal: [encoder setFragmentTexture:atIndex:]
	// Vulkan: update descriptor set binding.
	// OpenGL: glBindTextureUnit(slot, textureID).
	virtual void SetTexture(uint32_t slot, const Ref<ITexture2D> &texture) = 0;

	virtual void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0) = 0;
	virtual void DrawArrays(uint32_t mode, uint32_t first, uint32_t count) = 0;
};
