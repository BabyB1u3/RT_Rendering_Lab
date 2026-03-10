#include <gtest/gtest.h>
#include <glm/glm.hpp>

#include "MathTestUtils.h"

#include "scene/Camera.h"
#include "scene/DebugCameraController.h"

TEST(DebugCameraControllerTests, DefaultConstructedControllerHasNoCamera)
{
    DebugCameraController controller;
    EXPECT_EQ(controller.GetCamera(), nullptr);
}

TEST(DebugCameraControllerTests, ConstructorStoresCameraPointer)
{
    Camera camera;
    DebugCameraController controller(&camera);

    EXPECT_EQ(controller.GetCamera(), &camera);
}

TEST(DebugCameraControllerTests, SetCameraRebindsController)
{
    Camera cameraA;
    Camera cameraB;

    DebugCameraController controller(&cameraA);
    EXPECT_EQ(controller.GetCamera(), &cameraA);

    controller.SetCamera(&cameraB);
    EXPECT_EQ(controller.GetCamera(), &cameraB);
}

TEST(DebugCameraControllerTests, MoveSpeedSetterGetterWorks)
{
    DebugCameraController controller;
    controller.SetMoveSpeed(12.5f);

    EXPECT_FLOAT_EQ(controller.GetMoveSpeed(), 12.5f);
}

TEST(DebugCameraControllerTests, MouseSensitivitySetterGetterWorks)
{
    DebugCameraController controller;
    controller.SetMouseSensitivity(0.25f);

    EXPECT_FLOAT_EQ(controller.GetMouseSensitivity(), 0.25f);
}

TEST(DebugCameraControllerTests, ScrollSensitivitySetterGetterWorks)
{
    DebugCameraController controller;
    controller.SetScrollSensitivity(3.5f);

    EXPECT_FLOAT_EQ(controller.GetScrollSensitivity(), 3.5f);
}

TEST(DebugCameraControllerTests, MoveForwardMovesAlongCameraForward)
{
    Camera camera;
    camera.SetPosition({0.0f, 0.0f, 0.0f});

    DebugCameraController controller(&camera);
    controller.SetMoveSpeed(10.0f);

    const glm::vec3 start = camera.GetPosition();
    const glm::vec3 forward = camera.GetForward();

    controller.MoveForward(0.5f);

    const glm::vec3 expected = start + forward * 5.0f;
    ExpectVec3Near(camera.GetPosition(), expected);
}

TEST(DebugCameraControllerTests, MoveBackwardMovesOppositeToCameraForward)
{
    Camera camera;
    camera.SetPosition({1.0f, 2.0f, 3.0f});

    DebugCameraController controller(&camera);
    controller.SetMoveSpeed(8.0f);

    const glm::vec3 start = camera.GetPosition();
    const glm::vec3 forward = camera.GetForward();

    controller.MoveBackward(0.25f);

    const glm::vec3 expected = start - forward * 2.0f;
    ExpectVec3Near(camera.GetPosition(), expected);
}

TEST(DebugCameraControllerTests, MoveRightMovesAlongCameraRight)
{
    Camera camera;
    camera.SetPosition({0.0f, 0.0f, 0.0f});

    DebugCameraController controller(&camera);
    controller.SetMoveSpeed(6.0f);

    const glm::vec3 start = camera.GetPosition();
    const glm::vec3 right = camera.GetRight();

    controller.MoveRight(0.5f);

    const glm::vec3 expected = start + right * 3.0f;
    ExpectVec3Near(camera.GetPosition(), expected);
}

TEST(DebugCameraControllerTests, MoveLeftMovesOppositeToCameraRight)
{
    Camera camera;
    camera.SetPosition({3.0f, 4.0f, 5.0f});

    DebugCameraController controller(&camera);
    controller.SetMoveSpeed(4.0f);

    const glm::vec3 start = camera.GetPosition();
    const glm::vec3 right = camera.GetRight();

    controller.MoveLeft(0.75f);

    const glm::vec3 expected = start - right * 3.0f;
    ExpectVec3Near(camera.GetPosition(), expected);
}

TEST(DebugCameraControllerTests, MoveUpUsesWorldUpAxis)
{
    Camera camera;
    camera.SetPosition({1.0f, 2.0f, 3.0f});
    camera.SetRotation(-30.0f, 25.0f); // 故意让 camera up != world up

    DebugCameraController controller(&camera);
    controller.SetMoveSpeed(5.0f);

    const glm::vec3 start = camera.GetPosition();

    controller.MoveUp(0.4f);

    const glm::vec3 expected = start + glm::vec3(0.0f, 1.0f, 0.0f) * 2.0f;
    ExpectVec3Near(camera.GetPosition(), expected);
}

TEST(DebugCameraControllerTests, MoveDownUsesWorldUpAxis)
{
    Camera camera;
    camera.SetPosition({1.0f, 2.0f, 3.0f});
    camera.SetRotation(-30.0f, 25.0f);

    DebugCameraController controller(&camera);
    controller.SetMoveSpeed(5.0f);

    const glm::vec3 start = camera.GetPosition();

    controller.MoveDown(0.4f);

    const glm::vec3 expected = start - glm::vec3(0.0f, 1.0f, 0.0f) * 2.0f;
    ExpectVec3Near(camera.GetPosition(), expected);
}

TEST(DebugCameraControllerTests, MouseDeltaUsesSensitivity)
{
    Camera camera;
    DebugCameraController controller(&camera);

    controller.SetMouseSensitivity(0.5f);

    const float yaw0 = camera.GetYaw();
    const float pitch0 = camera.GetPitch();

    controller.OnMouseDelta(10.0f, -4.0f);

    EXPECT_NEAR(camera.GetYaw(), yaw0 + 5.0f, 1e-4f);
    EXPECT_NEAR(camera.GetPitch(), pitch0 - 2.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseDeltaConstrainsPitchToUpperBound)
{
    Camera camera;
    camera.SetRotation(0.0f, 80.0f);

    DebugCameraController controller(&camera);
    controller.SetMouseSensitivity(1.0f);

    controller.OnMouseDelta(0.0f, 20.0f, true);

    EXPECT_NEAR(camera.GetPitch(), 89.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseDeltaConstrainsPitchToLowerBound)
{
    Camera camera;
    camera.SetRotation(0.0f, -80.0f);

    DebugCameraController controller(&camera);
    controller.SetMouseSensitivity(1.0f);

    controller.OnMouseDelta(0.0f, -20.0f, true);

    EXPECT_NEAR(camera.GetPitch(), -89.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseDeltaCanSkipPitchConstraint)
{
    Camera camera;
    camera.SetRotation(0.0f, 80.0f);

    DebugCameraController controller(&camera);
    controller.SetMouseSensitivity(1.0f);

    controller.OnMouseDelta(0.0f, 20.0f, false);

    EXPECT_NEAR(camera.GetPitch(), 100.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseScrollDecreasesFovForPositiveScroll)
{
    Camera camera;
    camera.SetPerspective(45.0f, camera.GetAspectRatio(), camera.GetNearClip(), camera.GetFarClip());

    DebugCameraController controller(&camera);
    controller.SetScrollSensitivity(2.0f);

    controller.OnMouseScroll(3.0f);

    EXPECT_NEAR(camera.GetVerticalFovDegrees(), 39.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseScrollIncreasesFovForNegativeScroll)
{
    Camera camera;
    camera.SetPerspective(45.0f, camera.GetAspectRatio(), camera.GetNearClip(), camera.GetFarClip());

    DebugCameraController controller(&camera);
    controller.SetScrollSensitivity(4.0f);

    controller.OnMouseScroll(-2.0f);

    EXPECT_NEAR(camera.GetVerticalFovDegrees(), 53.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseScrollClampsFovToMinimum)
{
    Camera camera;
    camera.SetPerspective(5.0f, camera.GetAspectRatio(), camera.GetNearClip(), camera.GetFarClip());

    DebugCameraController controller(&camera);
    controller.SetScrollSensitivity(10.0f);

    controller.OnMouseScroll(1.0f);

    EXPECT_NEAR(camera.GetVerticalFovDegrees(), 1.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MouseScrollClampsFovToMaximum)
{
    Camera camera;
    camera.SetPerspective(85.0f, camera.GetAspectRatio(), camera.GetNearClip(), camera.GetFarClip());

    DebugCameraController controller(&camera);
    controller.SetScrollSensitivity(10.0f);

    controller.OnMouseScroll(-1.0f);

    EXPECT_NEAR(camera.GetVerticalFovDegrees(), 90.0f, 1e-4f);
}

TEST(DebugCameraControllerTests, MoveFunctionsDoNothingWhenCameraIsNull)
{
    DebugCameraController controller;
    controller.SetMoveSpeed(100.0f);

    EXPECT_NO_THROW(controller.MoveForward(1.0f));
    EXPECT_NO_THROW(controller.MoveBackward(1.0f));
    EXPECT_NO_THROW(controller.MoveLeft(1.0f));
    EXPECT_NO_THROW(controller.MoveRight(1.0f));
    EXPECT_NO_THROW(controller.MoveUp(1.0f));
    EXPECT_NO_THROW(controller.MoveDown(1.0f));
}

TEST(DebugCameraControllerTests, MouseFunctionsDoNothingWhenCameraIsNull)
{
    DebugCameraController controller;
    controller.SetMouseSensitivity(100.0f);
    controller.SetScrollSensitivity(100.0f);

    EXPECT_NO_THROW(controller.OnMouseDelta(10.0f, 20.0f));
    EXPECT_NO_THROW(controller.OnMouseDelta(10.0f, 20.0f, false));
    EXPECT_NO_THROW(controller.OnMouseScroll(3.0f));
}