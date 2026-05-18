#pragma once

/// @file FrameGlobals.h
/// @brief Per-frame renderer parameters shared by forward-rendered objects.

#include "Core/Util/Math.h"

namespace Renderer
{
struct FrameGlobals
{
    Math::Mat4 m_ViewProjection = Math::Mat4::Identity();
    Math::Vec4 m_Tint = Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float m_Time = 0.0f;
};
} // namespace Renderer
