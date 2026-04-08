#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <gtest/gtest.h>

inline void ExpectFloatNear(float a, float b, float eps = 1e-4f)
{
    EXPECT_NEAR(a, b, eps);
}

inline void ExpectVec3Near(const glm::vec3 &a, const glm::vec3 &b, float eps = 1e-4f)
{
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
}

inline void ExpectMat4Near(const glm::mat4 &a, const glm::mat4 &b, float eps = 1e-4f)
{
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            EXPECT_NEAR(a[c][r], b[c][r], eps);
        }
    }
}