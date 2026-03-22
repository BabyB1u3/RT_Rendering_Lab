#pragma once

/// @file RenderCommand.h
/// @brief Stateless wrappers around common OpenGL state and draw commands.
///
/// All methods are static — RenderCommand has no state of its own.
/// Init() sets sensible GL defaults (blend, depth test).
/// Render passes call these instead of raw gl* functions for readability.

#include <glm/glm.hpp>

#include "core/Base.h"
#include "VertexArray.h"

class RenderCommand
{
public:
    /// Set initial GL state: enable blending (src alpha, one-minus-src alpha) and depth test.
    static void Init();

    static void SetClearColor(const glm::vec4 &color);
    /// Clear the active framebuffer. Each flag controls one buffer bit.
    static void Clear(bool color = true, bool depth = true, bool stencil = false);

    static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    static void EnableDepthTest(bool enabled);
    static void EnableBlend(bool enabled);
    static void EnableCullFace(bool enabled);
    /// @param front  true = cull front faces (used in shadow pass), false = cull back faces.
    static void SetCullFace(bool front);

    /// Draw indexed triangles. If indexCount is 0, uses the full index buffer.
    static void DrawIndexed(const Ref<VertexArray> &vao, uint32_t indexCount = 0);
    static void DrawArrays(uint32_t mode, uint32_t first, uint32_t count);
};