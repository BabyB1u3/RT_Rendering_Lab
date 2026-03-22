#pragma once

/// @file Transform.h
/// @brief TRS (Translation-Rotation-Scale) transform for scene objects.
///
/// GetMatrix() computes the 4x4 model matrix as:  Translation * RotationZYX * Scale.
/// Rotation order is ZYX (roll, then yaw, then pitch) — standard for FPS-style objects.
/// The matrix is recomputed on every call; caching can be added if profiling shows a need.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    glm::vec3 RotationEulerDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f, 1.0f, 1.0f};

    /// Compute the TRS model matrix. Rotation order: Z * Y * X.
    glm::mat4 GetMatrix() const
    {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), Position);

        glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), glm::radians(RotationEulerDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(RotationEulerDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), glm::radians(RotationEulerDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::mat4 rotation = rotationZ * rotationY * rotationX;
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), Scale);

        return translation * rotation * scale;
    }
};