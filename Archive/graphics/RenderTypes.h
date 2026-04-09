#pragma once

/// @file RenderTypes.h
/// @brief Backend-agnostic render pipeline types: render pass descriptors, pipeline state.
///
/// These types replace the implicit state-setting pattern (SetClearColor + Clear,
/// EnableDepthTest, etc.) with explicit descriptors that map cleanly to Metal
/// (MTLRenderPassDescriptor, MTLRenderPipelineState) and Vulkan
/// (VkRenderPassBeginInfo, VkPipeline).

#include <cstdint>

#include <glm/glm.hpp>

/// What to do with an attachment's contents at the start of a render pass.
enum class LoadAction
{
	Load,	 ///< Preserve existing contents (Metal: MTLLoadActionLoad)
	Clear,	 ///< Clear to a specified value   (Metal: MTLLoadActionClear)
	DontCare ///< Contents are undefined       (Metal: MTLLoadActionDontCare)
};

/// What to do with an attachment's contents at the end of a render pass.
enum class StoreAction
{
	Store,	 ///< Write results to the attachment texture (Metal: MTLStoreActionStore)
	DontCare ///< Contents may be discarded              (Metal: MTLStoreActionDontCare)
};

/// Describes how a render pass should initialize and finalize its attachments.
/// Replaces the separate SetClearColor() + Clear() calls with a single descriptor
/// that is provided at the start of the pass.
struct RenderPassDescriptor
{
	// --- Color attachment ---
	LoadAction ColorLoadAction = LoadAction::Clear;
	StoreAction ColorStoreAction = StoreAction::Store;
	glm::vec4 ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};

	// --- Depth attachment ---
	LoadAction DepthLoadAction = LoadAction::Clear;
	StoreAction DepthStoreAction = StoreAction::Store;
	float ClearDepth = 1.0f;

	// --- Stencil attachment ---
	LoadAction StencilLoadAction = LoadAction::DontCare;
	StoreAction StencilStoreAction = StoreAction::DontCare;
	uint32_t ClearStencil = 0;
};

/// Immutable render state that Metal/Vulkan bake into a pipeline state object.
/// Replaces per-call EnableDepthTest(), EnableBlend(), EnableCullFace(), SetCullFace().
///
/// OpenGL applies these as individual glEnable/glDisable calls.
/// Metal/Vulkan compile them into MTLRenderPipelineState / VkPipeline at creation time
/// and cache the result.
struct PipelineState
{
	bool DepthTestEnabled = true;
	bool DepthWriteEnabled = true;
	bool BlendEnabled = false;
	bool CullFaceEnabled = true;
	bool CullFront = false; ///< false = cull back faces (default), true = cull front
};
