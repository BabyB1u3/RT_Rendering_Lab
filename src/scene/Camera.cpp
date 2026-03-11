#include "Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

Camera::Camera(float verticalFovDegrees, float aspectRatio, float nearClip, float farClip)
    : m_VerticalFovDegrees(verticalFovDegrees),
      m_AspectRatio(aspectRatio),
      m_NearClip(nearClip),
      m_FarClip(farClip)
{
    RecalculateBasis();
    RecalculateProjection();
    RecalculateView();
}

void Camera::SetPerspective(float verticalFovDegrees, float aspectRatio, float nearClip, float farClip)
{
    m_VerticalFovDegrees = verticalFovDegrees;
    m_AspectRatio = aspectRatio;
    m_NearClip = nearClip;
    m_FarClip = farClip;
    RecalculateProjection();
}

void Camera::SetViewportSize(uint32_t width, uint32_t height)
{
    if (height == 0)
        return;

    m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
    RecalculateProjection();
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_AspectRatio = aspectRatio;
    RecalculateProjection();
}

void Camera::SetPosition(const glm::vec3 &position)
{
    m_Position = position;
    RecalculateView();
}

void Camera::SetRotation(float yawDegrees, float pitchDegrees)
{
    m_Yaw = yawDegrees;
    // Camera could have a lot of purposes:
    // Player controller, orbit camera, light camera ... etc.
    // Some controller may don't want to clamp this value
    // m_Pitch = std::clamp(pitchDegrees, -89.0f, 89.0f);
    m_Pitch = pitchDegrees;

    RecalculateBasis();
    RecalculateView();
}

void Camera::RecalculateProjection()
{
    m_Projection = glm::perspective(
        glm::radians(m_VerticalFovDegrees),
        m_AspectRatio,
        m_NearClip,
        m_FarClip);
}

void Camera::RecalculateView()
{
    m_View = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);
}

void Camera::RecalculateBasis()
{
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));
    forward.y = std::sin(glm::radians(m_Pitch));
    forward.z = std::sin(glm::radians(m_Yaw)) * std::cos(glm::radians(m_Pitch));

    m_Forward = glm::normalize(forward);
    m_Right = glm::normalize(glm::cross(m_Forward, m_WorldUp));
    m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
}