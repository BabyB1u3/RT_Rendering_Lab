#include <gtest/gtest.h>

#include <cmath>

#include "Core/Util/Math.h"
#include "MathTestUtils.h"
#include "Scene/Camera.h"

TEST(CameraTests, DefaultBasisVectorsAreOrthogonalAndNormalized)
{
    Camera camera;

    EXPECT_NEAR(camera.GetForward().norm(), 1.0f, 1e-4f);
    EXPECT_NEAR(camera.GetRight().norm(), 1.0f, 1e-4f);
    EXPECT_NEAR(camera.GetUp().norm(), 1.0f, 1e-4f);

    EXPECT_NEAR(camera.GetForward().dot(camera.GetRight()), 0.0f, 1e-4f);
    EXPECT_NEAR(camera.GetForward().dot(camera.GetUp()), 0.0f, 1e-4f);
    EXPECT_NEAR(camera.GetRight().dot(camera.GetUp()), 0.0f, 1e-4f);
}

TEST(CameraTests, DefaultConstructionBuildsConsistentViewAndProjection)
{
    Camera camera;

    Math::Mat4 expectedView =
        Math::LookAt(camera.GetPosition(), camera.GetPosition() + camera.GetForward(), camera.GetUp());
    Math::Mat4 expectedProjection = Math::PerspectiveRH_ZO(Math::Radians(camera.GetVerticalFovDegrees()),
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
    const Math::Mat4 originalProjection = camera.GetProjection();

    camera.SetViewportSize(1920, 0);

    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), originalAspect);
    ExpectMat4Near(camera.GetProjection(), originalProjection);
}

TEST(CameraTests, ExtremePitchKeepsBasisAndViewFinite)
{
    Camera camera;
    camera.SetRotation(-90.0f, 90.0f);

    EXPECT_TRUE(camera.GetForward().allFinite());
    EXPECT_TRUE(camera.GetRight().allFinite());
    EXPECT_TRUE(camera.GetUp().allFinite());

    EXPECT_NEAR(camera.GetForward().squaredNorm(), 1.0f, 1e-3f);
    EXPECT_NEAR(camera.GetRight().squaredNorm(), 1.0f, 1e-3f);
    EXPECT_NEAR(camera.GetUp().squaredNorm(), 1.0f, 1e-3f);

    EXPECT_TRUE(camera.GetView().allFinite());
}

TEST(CameraTests, SetAspectRatioUpdatesProjectionState)
{
    Camera camera(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    auto before = camera.GetProjection();
    camera.SetAspectRatio(1.0f);
    auto after = camera.GetProjection();

    EXPECT_NE(before(0, 0), after(0, 0));
    EXPECT_FLOAT_EQ(camera.GetAspectRatio(), 1.0f);
}

TEST(CameraTests, ViewProjectionMatchesProjectionTimesView)
{
    Camera camera(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);
    camera.SetPosition({1.0f, 2.0f, 3.0f});
    camera.SetRotation(-90.0f, 0.0f);

    Math::Mat4 expected = camera.GetProjection() * camera.GetView();
    ExpectMat4Near(camera.GetViewProjection(), expected);
}
