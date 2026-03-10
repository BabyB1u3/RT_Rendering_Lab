#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include "scene/Camera.h"
#include "MathTestUtils.h"

TEST(CameraTests, DefaultBasisVectorsAreOrthogonalAndNormalized)
{
    Camera camera;

    EXPECT_NEAR(glm::length(camera.GetForward()), 1.0f, 1e-4f);
    EXPECT_NEAR(glm::length(camera.GetRight()), 1.0f, 1e-4f);
    EXPECT_NEAR(glm::length(camera.GetUp()), 1.0f, 1e-4f);

    EXPECT_NEAR(glm::dot(camera.GetForward(), camera.GetRight()), 0.0f, 1e-4f);
    EXPECT_NEAR(glm::dot(camera.GetForward(), camera.GetUp()), 0.0f, 1e-4f);
    EXPECT_NEAR(glm::dot(camera.GetRight(), camera.GetUp()), 0.0f, 1e-4f);
}

TEST(CameraTests, SetPositionUpdatesStoredPosition)
{
    Camera camera;
    camera.SetPosition({4.0f, 5.0f, 6.0f});

    ExpectVec3Near(camera.GetPosition(), {4.0f, 5.0f, 6.0f});
}

TEST(CameraTests, SetRotationUpdatesYawAndPitch)
{
    Camera camera;
    camera.SetRotation(-45.0f, 20.0f);

    EXPECT_NEAR(camera.GetYaw(), -45.0f, 1e-4f);
    EXPECT_NEAR(camera.GetPitch(), 20.0f, 1e-4f);
}

TEST(CameraTests, SetAspectRatioUpdatesProjectionState)
{
    Camera camera(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    auto before = camera.GetProjection();
    camera.SetAspectRatio(1.0f);
    auto after = camera.GetProjection();

    EXPECT_NE(before[0][0], after[0][0]);
    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), 1.0f);
}

TEST(CameraTests, ViewProjectionMatchesProjectionTimesView)
{
    Camera camera(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);
    camera.SetPosition({1.0f, 2.0f, 3.0f});
    camera.SetRotation(-90.0f, 0.0f);

    glm::mat4 expected = camera.GetProjection() * camera.GetView();
    ExpectMat4Near(camera.GetViewProjection(), expected);
}