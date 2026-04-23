#pragma once

#include <cmath>
#include <Eigen/Core>
#include <gtest/gtest.h>

inline void ExpectFloatNear(float a, float b, float eps = 1e-4f)
{
    EXPECT_NEAR(a, b, eps);
}

inline void ExpectVec3Near(const Eigen::Vector3f& a, const Eigen::Vector3f& b, float eps = 1e-4f)
{
    EXPECT_NEAR(a.x(), b.x(), eps);
    EXPECT_NEAR(a.y(), b.y(), eps);
    EXPECT_NEAR(a.z(), b.z(), eps);
}

inline void ExpectMat4Near(const Eigen::Matrix4f& a, const Eigen::Matrix4f& b, float eps = 1e-4f)
{
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            EXPECT_NEAR(a(r, c), b(r, c), eps);
        }
    }
}
