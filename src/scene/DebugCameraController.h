#pragma once

#include "Camera.h"

class DebugCameraController
{
public:
    DebugCameraController() = default;
    explicit DebugCameraController(Camera *camera);

    void SetCamera(Camera *camera);
    Camera *GetCamera() const { return m_Camera; }

    void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
    void SetScrollSensitivity(float sensitivity) { m_ScrollSensitivity = sensitivity; }

    float GetMoveSpeed() const { return m_MoveSpeed; }
    float GetMouseSensitivity() const { return m_MouseSensitivity; }
    float GetScrollSensitivity() const { return m_ScrollSensitivity; }

    // Per-frame movement input
    void MoveForward(float deltaTime);
    void MoveBackward(float deltaTime);
    void MoveLeft(float deltaTime);
    void MoveRight(float deltaTime);
    void MoveUp(float deltaTime);
    void MoveDown(float deltaTime);

    // Mouse delta
    void OnMouseDelta(float deltaX, float deltaY, bool constrainPitch = true);

    // Scroll delta (typically y offset)
    void OnMouseScroll(float deltaScroll);

private:
    // Non-owning pointer. The controller does not manage camera lifetime.
    Camera *m_Camera = nullptr;

    float m_MoveSpeed = 5.0f;
    float m_MouseSensitivity = 0.1f;
    float m_ScrollSensitivity = 1.0f;
};