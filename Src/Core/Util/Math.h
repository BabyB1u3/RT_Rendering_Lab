#pragma once

/// @file Math.h
/// @brief Small math helpers over Eigen for the engine's render-facing math conventions.
///
/// The engine keeps Eigen as the implementation layer but centralizes its
/// render-facing aliases and projection helpers here so backend-sensitive
/// choices stay explicit. For Vulkan/Metal-facing rendering code, prefer the
/// explicit RH_ZO helpers rather than relying on historical clip-space defaults.

#include <cmath>
#include <numbers>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace Math
{

// Project-local math aliases used across the codebase.
// Prefer these names at call sites so Eigen remains an implementation detail.
using Vec2 = Eigen::Vector2f;
using Vec3 = Eigen::Vector3f;
using Vec4 = Eigen::Vector4f;
using Mat3 = Eigen::Matrix3f;
using Mat4 = Eigen::Matrix4f;
using Quat = Eigen::Quaternionf;

inline float Radians(float degrees)
{
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

/// Post-multiply `m` by a translation matrix built from `v`.
inline Mat4 Translate(const Mat4& m, const Vec3& v)
{
    Mat4 result = m;
    result.col(3) = m.col(0) * v.x() + m.col(1) * v.y() + m.col(2) * v.z() + m.col(3);
    return result;
}

/// Post-multiply `m` by a rotation around `axis` by `angleRadians`.
inline Mat4 Rotate(const Mat4& m, float angleRadians, const Vec3& axis)
{
    Mat4 rotation = Mat4::Identity();
    rotation.topLeftCorner<3, 3>() = Eigen::AngleAxisf(angleRadians, axis.normalized()).toRotationMatrix();
    return m * rotation;
}

/// Post-multiply `m` by a non-uniform scale.
inline Mat4 Scale(const Mat4& m, const Vec3& s)
{
    Mat4 result;
    result.col(0) = m.col(0) * s.x();
    result.col(1) = m.col(1) * s.y();
    result.col(2) = m.col(2) * s.z();
    result.col(3) = m.col(3);
    return result;
}

/// Right-handed perspective projection with NDC z in [0, 1].
/// Prefer this for the engine's Vulkan/Metal render path.
inline Mat4 PerspectiveRH_ZO(float fovyRadians, float aspect, float zNear, float zFar)
{
    const float tanHalfFovy = std::tan(fovyRadians * 0.5f);

    Mat4 result = Mat4::Zero();
    result(0, 0) = 1.0f / (aspect * tanHalfFovy);
    result(1, 1) = 1.0f / tanHalfFovy;
    result(2, 2) = -zFar / (zFar - zNear);
    result(3, 2) = -1.0f;
    result(2, 3) = -(zFar * zNear) / (zFar - zNear);
    return result;
}

/// Right-handed perspective projection with NDC z in [-1, 1].
/// Use this only when interoperating with code that explicitly expects the legacy RH_NO convention.
inline Mat4 PerspectiveRH_NO(float fovyRadians, float aspect, float zNear, float zFar)
{
    const float tanHalfFovy = std::tan(fovyRadians * 0.5f);

    Mat4 result = Mat4::Zero();
    result(0, 0) = 1.0f / (aspect * tanHalfFovy);
    result(1, 1) = 1.0f / tanHalfFovy;
    result(2, 2) = -(zFar + zNear) / (zFar - zNear);
    result(3, 2) = -1.0f;
    result(2, 3) = -(2.0f * zFar * zNear) / (zFar - zNear);
    return result;
}

/// Right-handed look-at view matrix.
inline Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
{
    const Vec3 f = (center - eye).normalized();
    const Vec3 s = f.cross(up).normalized();
    const Vec3 u = s.cross(f);

    Mat4 result = Mat4::Identity();
    result(0, 0) = s.x();
    result(0, 1) = s.y();
    result(0, 2) = s.z();
    result(1, 0) = u.x();
    result(1, 1) = u.y();
    result(1, 2) = u.z();
    result(2, 0) = -f.x();
    result(2, 1) = -f.y();
    result(2, 2) = -f.z();
    result(0, 3) = -s.dot(eye);
    result(1, 3) = -u.dot(eye);
    result(2, 3) = f.dot(eye);
    return result;
}

} // namespace Math
