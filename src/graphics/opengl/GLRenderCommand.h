#pragma once

/// @file GLRenderCommand.h
/// @brief OpenGL implementation of IRenderCommand.
///
/// Instance-based version of the current static RenderCommand class.

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

	void SetClearColor(const glm::vec4 &color) override;
	void Clear(bool color = true, bool depth = true, bool stencil = false) override;

	void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

	void EnableDepthTest(bool enabled) override;
	void EnableBlend(bool enabled) override;
	void EnableCullFace(bool enabled) override;
	void SetCullFace(bool front) override;

	void DrawIndexed(const Ref<IVertexArray> &vao, uint32_t indexCount = 0) override;
	void DrawArrays(uint32_t mode, uint32_t first, uint32_t count) override;
};
