#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <cmath>

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

TEST(CameraTests, DefaultConstructionBuildsConsistentViewAndProjection)
{
    Camera camera;

    glm::mat4 expectedView = glm::lookAt(
        camera.GetPosition(),
        camera.GetPosition() + camera.GetForward(),
        camera.GetUp());
    glm::mat4 expectedProjection = glm::perspective(
        glm::radians(camera.GetVerticalFovDegrees()),
        camera.GetAspectRatio(),
        camera.GetNearClip(),
        camera.GetFarClip());

    ExpectMat4Near(camera.GetView(), expectedView);
    ExpectMat4Near(camera.GetProjection(), expectedProjection);
    ExpectMat4Near(camera.GetViewProjection(), expectedProjection * expectedView);
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

TEST(CameraTests, SetViewportSizeWithZeroHeightKeepsProjectionStateUnchanged)
{
    Camera camera(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    const float originalAspect = camera.GetAspectRatio();
    const glm::mat4 originalProjection = camera.GetProjection();

    camera.SetViewportSize(1920, 0);

    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), originalAspect);
    ExpectMat4Near(camera.GetProjection(), originalProjection);
}

TEST(CameraTests, ExtremePitchKeepsBasisAndViewFinite)
{
    auto isFiniteVec3 = [](const glm::vec3 &value)
    {
        return std::isfinite(value.x) &&
               std::isfinite(value.y) &&
               std::isfinite(value.z);
    };
    auto isFiniteVec4 = [](const glm::vec4 &value)
    {
        return std::isfinite(value.x) &&
               std::isfinite(value.y) &&
               std::isfinite(value.z) &&
               std::isfinite(value.w);
    };

    Camera camera;
    camera.SetRotation(-90.0f, 90.0f);

    EXPECT_TRUE(isFiniteVec3(camera.GetForward()));
    EXPECT_TRUE(isFiniteVec3(camera.GetRight()));
    EXPECT_TRUE(isFiniteVec3(camera.GetUp()));

    EXPECT_NEAR(glm::length2(camera.GetForward()), 1.0f, 1e-3f);
    EXPECT_NEAR(glm::length2(camera.GetRight()), 1.0f, 1e-3f);
    EXPECT_NEAR(glm::length2(camera.GetUp()), 1.0f, 1e-3f);

    const glm::mat4 &view = camera.GetView();
    for (int c = 0; c < 4; ++c)
        EXPECT_TRUE(isFiniteVec4(view[c]));
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
