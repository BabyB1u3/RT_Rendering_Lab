#pragma once

/// @file Camera.h
/// @brief Perspective camera with yaw/pitch Euler rotation.
///
/// The camera maintains its own projection and view matrices, recomputed
/// automatically when position, rotation, FOV, or aspect ratio change.
///
/// Orientation is defined by yaw (horizontal, degrees) and pitch (vertical, degrees).
/// Yaw = -90 looks along -Z in this right-handed camera setup. Pitch is NOT clamped internally -
/// clamping is the responsibility of the controller (e.g., DebugCameraController)
/// so that specialized cameras (orbit, light) can use the full range.
///
/// Basis vectors (Forward, Right, Up) are derived from yaw/pitch via RecalculateBasis().

#include <Eigen/Core>

class Camera
{
public:
    Camera();
    Camera(float verticalFovDegrees, float aspectRatio, float nearClip, float farClip);

    void SetPerspective(float verticalFovDegrees, float aspectRatio, float nearClip, float farClip);
    void SetViewportSize(uint32_t width, uint32_t height);
    void SetAspectRatio(float aspectRatio);

    void SetPosition(const Eigen::Vector3f& position);
    void SetRotation(float yawDegrees, float pitchDegrees);

    const Eigen::Vector3f& GetPosition() const { return m_Position; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }

    const Eigen::Vector3f& GetForward() const { return m_Forward; }
    const Eigen::Vector3f& GetRight() const { return m_Right; }
    const Eigen::Vector3f& GetUp() const { return m_Up; }

    const Eigen::Matrix4f& GetProjection() const { return m_Projection; }
    const Eigen::Matrix4f& GetView() const { return m_View; }
    Eigen::Matrix4f GetViewProjection() const { return m_Projection * m_View; }

    float GetVerticalFovDegrees() const { return m_VerticalFovDegrees; }
    float GetAspectRatio() const { return m_AspectRatio; }
    float GetNearClip() const { return m_NearClip; }
    float GetFarClip() const { return m_FarClip; }

    void RecalculateProjection();
    void RecalculateView();

private:
    void RecalculateBasis();

private:
    // Perspective settings
    float m_VerticalFovDegrees = 45.0f;
    float m_AspectRatio = 16.0f / 9.0f;
    float m_NearClip = 0.1f;
    float m_FarClip = 100.0f;

    // Transform
    Eigen::Vector3f m_Position{0.0f, 0.0f, 3.0f};
    float m_Yaw = -90.0f;
    float m_Pitch = 0.0f;

    // Basis vectors
    Eigen::Vector3f m_Forward{0.0f, 0.0f, -1.0f};
    Eigen::Vector3f m_Right{1.0f, 0.0f, 0.0f};
    Eigen::Vector3f m_Up{0.0f, 1.0f, 0.0f};
    Eigen::Vector3f m_WorldUp{0.0f, 1.0f, 0.0f};

    Eigen::Matrix4f m_Projection = Eigen::Matrix4f::Identity();
    Eigen::Matrix4f m_View = Eigen::Matrix4f::Identity();
};
