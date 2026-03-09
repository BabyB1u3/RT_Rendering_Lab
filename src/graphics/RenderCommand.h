#pragma once

#include <glm/glm.hpp>

#include "VertexArray.h"

// RenderCommand Class
class RenderCommand
{

public:
	static void Init();

	// Set the clear color and clear the color buffer with it
	// Clear the depth buffer at the same time
	static void Clear(const glm::vec4 &color);

	// Draw stuffs with VBO and IBO
	static void DrawIndexed(const Ref<VAO> &vao, uint32_t indexCount = 0);

	static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

private:
};
