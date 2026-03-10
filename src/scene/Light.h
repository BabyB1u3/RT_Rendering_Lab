#pragma once

#include <glm/glm.hpp>

struct DirectionalLight
{
    glm::vec3 Direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    float Intensity = 1.0f;

    glm::vec3 Color{1.0f, 1.0f, 1.0f};
    float Padding = 0.0f;
};