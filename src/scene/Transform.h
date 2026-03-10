#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
    glm::vec3 Position{0.0f, 0.0f, 0.0f};
    glm::vec3 RotationEulerDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 Scale{1.0f, 1.0f, 1.0f};

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