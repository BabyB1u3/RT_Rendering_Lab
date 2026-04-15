#pragma once

/// @file DebugCameraController.h
/// @brief First-person fly camera controller for debugging and exploration.
///
/// Translates WASD/QE key input into camera movement along the camera's basis
/// vectors (Forward, Right) and world Y axis (Up/Down). Mouse deltas rotate
/// yaw/pitch. Scroll wheel adjusts the vertical FOV.
///
/// The controller does NOT poll input itself - the demo calls the individual
/// Move*/OnMouse* methods each frame after reading from the Input system.
/// This keeps the controller independent of any specific input backend.
///
/// Non-owning: the controller holds a raw pointer to the Camera it drives.

#include "Scene/Camera.h"

class DebugCameraController
{
public:
    DebugCameraController() = default;
    explicit DebugCameraController(Camera* camera);

    void SetCamera(Camera* camera);
    Camera* GetCamera() const { return m_Camera; }

    void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
    void SetScrollSensitivity(float sensitivity) { m_ScrollSensitivity = sensitivity; }

    float GetMoveSpeed() const { return m_MoveSpeed; }
    float GetMouseSensitivity() const { return m_MouseSensitivity; }
    float GetScrollSensitivity() const { return m_ScrollSensitivity; }

    // Per-frame movement input
    void MoveForward(double deltaTime);
    void MoveBackward(double deltaTime);
    void MoveLeft(double deltaTime);
    void MoveRight(double deltaTime);
    // Along world's Y axis
    void MoveUp(double deltaTime);
    void MoveDown(double deltaTime);

    // Mouse delta
    // clamp to [-89, 89] when constrainPitch == true
    void OnMouseDelta(float deltaX, float deltaY, bool constrainPitch = true);

    // Scroll delta (typically y offset)
    // Revise verticalFov and clamp to [1, 90]
    void OnMouseScroll(float deltaScroll);

private:
    // Non-owning pointer. The controller does not manage camera lifetime.
    Camera* m_Camera = nullptr;

    float m_MoveSpeed = 5.0f;
    float m_MouseSensitivity = 0.1f;
    float m_ScrollSensitivity = 1.0f;
};
