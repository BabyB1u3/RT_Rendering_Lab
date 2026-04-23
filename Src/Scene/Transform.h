#pragma once

/// @file Transform.h
/// @brief TRS (Translation-Rotation-Scale) transform for scene objects.
///
/// GetMatrix() computes the 4x4 model matrix as:  Translation * RotationZYX * Scale.
/// Rotation order is ZYX (roll, then yaw, then pitch) - standard for FPS-style objects.
/// The matrix is recomputed on every call; caching can be added if profiling shows a need.

#include "Core/Util/Math.h"

struct Transform
{
    Math::Vec3 m_Position{0.0f, 0.0f, 0.0f};
    Math::Vec3 m_RotationEulerDegrees{0.0f, 0.0f, 0.0f};
    Math::Vec3 m_Scale{1.0f, 1.0f, 1.0f};

    /// Compute the TRS model matrix. Rotation order: Z * Y * X.
    Math::Mat4 GetMatrix() const
    {
        const Math::Mat4 translation = Math::Translate(Math::Mat4::Identity(), m_Position);

        const Math::Mat4 rotationX = Math::Rotate(
            Math::Mat4::Identity(), Math::Radians(m_RotationEulerDegrees.x()), Math::Vec3(1.0f, 0.0f, 0.0f));
        const Math::Mat4 rotationY = Math::Rotate(
            Math::Mat4::Identity(), Math::Radians(m_RotationEulerDegrees.y()), Math::Vec3(0.0f, 1.0f, 0.0f));
        const Math::Mat4 rotationZ = Math::Rotate(
            Math::Mat4::Identity(), Math::Radians(m_RotationEulerDegrees.z()), Math::Vec3(0.0f, 0.0f, 1.0f));

        const Math::Mat4 rotation = rotationZ * rotationY * rotationX;
        const Math::Mat4 scale = Math::Scale(Math::Mat4::Identity(), m_Scale);

        return translation * rotation * scale;
    }
};
