#pragma once

#include <glm/glm.hpp>

#include "VertexArray.h"

class RenderCommand
{
public:
    static void Init();

    static void SetClearColor(const glm::vec4 &color);
    static void Clear(bool color = true, bool depth = true, bool stencil = false);

    static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    static void EnableDepthTest(bool enabled);
    static void EnableBlend(bool enabled);
    static void EnableCullFace(bool enabled);

    static void DrawIndexed(const std::shared_ptr<VertexArray> &vao, uint32_t indexCount = 0);
    static void DrawArrays(uint32_t mode, uint32_t first, uint32_t count);
};