#include "DebugCameraController.h"

#include <algorithm>

DebugCameraController::DebugCameraController(Camera *camera)
    : m_Camera(camera)
{
}

void DebugCameraController::SetCamera(Camera *camera)
{
    m_Camera = camera;
}

void DebugCameraController::MoveForward(float deltaTime)
{
    if (!m_Camera)
        return;
    glm::vec3 position = m_Camera->GetPosition();
    position += m_Camera->GetForward() * (m_MoveSpeed * deltaTime);
    m_Camera->SetPosition(position);
}

void DebugCameraController::MoveBackward(float deltaTime)
{
    if (!m_Camera)
        return;
    glm::vec3 position = m_Camera->GetPosition();
    position -= m_Camera->GetForward() * (m_MoveSpeed * deltaTime);
    m_Camera->SetPosition(position);
}

void DebugCameraController::MoveLeft(float deltaTime)
{
    if (!m_Camera)
        return;
    glm::vec3 position = m_Camera->GetPosition();
    position -= m_Camera->GetRight() * (m_MoveSpeed * deltaTime);
    m_Camera->SetPosition(position);
}

void DebugCameraController::MoveRight(float deltaTime)
{
    if (!m_Camera)
        return;
    glm::vec3 position = m_Camera->GetPosition();
    position += m_Camera->GetRight() * (m_MoveSpeed * deltaTime);
    m_Camera->SetPosition(position);
}

void DebugCameraController::MoveUp(float deltaTime)
{
    if (!m_Camera)
        return;
    glm::vec3 position = m_Camera->GetPosition();
    position += glm::vec3(0.0f, 1.0f, 0.0f) * (m_MoveSpeed * deltaTime);
    m_Camera->SetPosition(position);
}

void DebugCameraController::MoveDown(float deltaTime)
{
    if (!m_Camera)
        return;
    glm::vec3 position = m_Camera->GetPosition();
    position -= glm::vec3(0.0f, 1.0f, 0.0f) * (m_MoveSpeed * deltaTime);
    m_Camera->SetPosition(position);
}

void DebugCameraController::OnMouseDelta(float deltaX, float deltaY, bool constrainPitch)
{
    if (!m_Camera)
        return;

    float yaw = m_Camera->GetYaw();
    float pitch = m_Camera->GetPitch();

    yaw += deltaX * m_MouseSensitivity;
    pitch += deltaY * m_MouseSensitivity;

    if (constrainPitch)
        pitch = std::clamp(pitch, -89.0f, 89.0f);

    m_Camera->SetRotation(yaw, pitch);
}

void DebugCameraController::OnMouseScroll(float deltaScroll)
{
    if (!m_Camera)
        return;

    float newFov = m_Camera->GetVerticalFovDegrees() - deltaScroll * m_ScrollSensitivity;
    newFov = std::clamp(newFov, 1.0f, 90.0f);

    m_Camera->SetPerspective(
        newFov,
        m_Camera->GetAspectRatio(),
        m_Camera->GetNearClip(),
        m_Camera->GetFarClip());
}