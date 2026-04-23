#pragma once

/// @file Transform.h
/// @brief TRS (Translation-Rotation-Scale) transform for scene objects.
///
/// GetMatrix() computes the 4x4 model matrix as:  Translation * RotationZYX * Scale.
/// Rotation order is ZYX (roll, then yaw, then pitch) - standard for FPS-style objects.
/// The matrix is recomputed on every call; caching can be added if profiling shows a need.

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "Core/Util/Math.h"

struct Transform
{
    Eigen::Vector3f m_Position{0.0f, 0.0f, 0.0f};
    Eigen::Vector3f m_RotationEulerDegrees{0.0f, 0.0f, 0.0f};
    Eigen::Vector3f m_Scale{1.0f, 1.0f, 1.0f};

    /// Compute the TRS model matrix. Rotation order: Z * Y * X.
    Eigen::Matrix4f GetMatrix() const
    {
        const Eigen::Matrix4f translation = Math::Translate(Eigen::Matrix4f::Identity(), m_Position);

        const Eigen::Matrix4f rotationX = Math::Rotate(
            Eigen::Matrix4f::Identity(), Math::Radians(m_RotationEulerDegrees.x()), Eigen::Vector3f(1.0f, 0.0f, 0.0f));
        const Eigen::Matrix4f rotationY = Math::Rotate(
            Eigen::Matrix4f::Identity(), Math::Radians(m_RotationEulerDegrees.y()), Eigen::Vector3f(0.0f, 1.0f, 0.0f));
        const Eigen::Matrix4f rotationZ = Math::Rotate(
            Eigen::Matrix4f::Identity(), Math::Radians(m_RotationEulerDegrees.z()), Eigen::Vector3f(0.0f, 0.0f, 1.0f));

        const Eigen::Matrix4f rotation = rotationZ * rotationY * rotationX;
        const Eigen::Matrix4f scale = Math::Scale(Eigen::Matrix4f::Identity(), m_Scale);

        return translation * rotation * scale;
    }
};
