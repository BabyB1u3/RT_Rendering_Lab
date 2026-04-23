#pragma once

/// @file Math.h
/// @brief Small math helpers over Eigen that replicate the glm-compatible
///        conventions previously used throughout the project.
///
/// All functions assume a right-handed coordinate system with NDC depth in
/// [-1, 1], matching glm's default RH_NO configuration so that matrices
/// produced here are bit-equivalent (up to floating-point rounding) to the
/// matrices the codebase produced before the glm->Eigen migration.

#include <cmath>
#include <numbers>

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace Math
{

inline float Radians(float degrees)
{
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

/// Post-multiply `m` by a translation matrix built from `v`. Matches glm::translate.
inline Eigen::Matrix4f Translate(const Eigen::Matrix4f& m, const Eigen::Vector3f& v)
{
    Eigen::Matrix4f result = m;
    result.col(3) = m.col(0) * v.x() + m.col(1) * v.y() + m.col(2) * v.z() + m.col(3);
    return result;
}

/// Post-multiply `m` by a rotation around `axis` by `angleRadians`. Matches glm::rotate.
inline Eigen::Matrix4f Rotate(const Eigen::Matrix4f& m, float angleRadians, const Eigen::Vector3f& axis)
{
    Eigen::Matrix4f rotation = Eigen::Matrix4f::Identity();
    rotation.topLeftCorner<3, 3>() = Eigen::AngleAxisf(angleRadians, axis.normalized()).toRotationMatrix();
    return m * rotation;
}

/// Post-multiply `m` by a non-uniform scale. Matches glm::scale.
inline Eigen::Matrix4f Scale(const Eigen::Matrix4f& m, const Eigen::Vector3f& s)
{
    Eigen::Matrix4f result;
    result.col(0) = m.col(0) * s.x();
    result.col(1) = m.col(1) * s.y();
    result.col(2) = m.col(2) * s.z();
    result.col(3) = m.col(3);
    return result;
}

/// Right-handed perspective projection with NDC z in [-1, 1]. Matches glm::perspective (RH_NO).
inline Eigen::Matrix4f Perspective(float fovyRadians, float aspect, float zNear, float zFar)
{
    const float tanHalfFovy = std::tan(fovyRadians * 0.5f);

    Eigen::Matrix4f result = Eigen::Matrix4f::Zero();
    result(0, 0) = 1.0f / (aspect * tanHalfFovy);
    result(1, 1) = 1.0f / tanHalfFovy;
    result(2, 2) = -(zFar + zNear) / (zFar - zNear);
    result(3, 2) = -1.0f;
    result(2, 3) = -(2.0f * zFar * zNear) / (zFar - zNear);
    return result;
}

/// Right-handed look-at view matrix. Matches glm::lookAt (RH).
inline Eigen::Matrix4f LookAt(const Eigen::Vector3f& eye, const Eigen::Vector3f& center, const Eigen::Vector3f& up)
{
    const Eigen::Vector3f f = (center - eye).normalized();
    const Eigen::Vector3f s = f.cross(up).normalized();
    const Eigen::Vector3f u = s.cross(f);

    Eigen::Matrix4f result = Eigen::Matrix4f::Identity();
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
