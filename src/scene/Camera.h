#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
    Camera() = default;
    Camera(float verticalFovDegrees, float aspectRatio, float nearClip, float farClip);

    void SetPerspective(float verticalFovDegrees, float aspectRatio, float nearClip, float farClip);
    void SetViewportSize(uint32_t width, uint32_t height);
    void SetAspectRatio(float aspectRatio);

    void SetPosition(const glm::vec3 &position);
    void SetRotation(float yawDegrees, float pitchDegrees);

    const glm::vec3 &GetPosition() const { return m_Position; }
    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }

    const glm::vec3 &GetForward() const { return m_Forward; }
    const glm::vec3 &GetRight() const { return m_Right; }
    const glm::vec3 &GetUp() const { return m_Up; }

    const glm::mat4 &GetProjection() const { return m_Projection; }
    const glm::mat4 &GetView() const { return m_View; }
    glm::mat4 GetViewProjection() const { return m_Projection * m_View; }

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
    glm::vec3 m_Position{0.0f, 0.0f, 3.0f};
    float m_Yaw = -90.0f;
    float m_Pitch = 0.0f;

    // Basis vectors
    glm::vec3 m_Forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_Right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_Up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_WorldUp{0.0f, 1.0f, 0.0f};

    glm::mat4 m_Projection{1.0f};
    glm::mat4 m_View{1.0f};
};