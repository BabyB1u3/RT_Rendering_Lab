#pragma once

/// @file IRenderCommand.h
/// @brief Abstract interface for render state and draw commands. Backend-agnostic.
///
/// The existing static RenderCommand class can become a forwarding shim
/// to the active IRenderCommand instance during R2 transition.

#include <cstdint>

#include <glm/glm.hpp>

#include "core/Base.h"

class IVertexArray;

class IRenderCommand
{
public:
	virtual ~IRenderCommand() = default;

	virtual void Init() = 0;

	virtual void SetClearColor(const glm::vec4 &color) = 0;
	virtual void Clear(bool color = true, bool depth = true, bool stencil = false) = 0;

	virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

	virtual void EnableDepthTest(bool enabled) = 0;
	virtual void EnableBlend(bool enabled) = 0;
	virtual void EnableCullFace(bool enabled) = 0;
	virtual void SetCullFace(bool front) = 0;

	virtual void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0) = 0;
	virtual void DrawArrays(uint32_t mode, uint32_t first, uint32_t count) = 0;
};
