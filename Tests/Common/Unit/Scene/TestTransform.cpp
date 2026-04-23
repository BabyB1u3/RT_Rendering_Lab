#include <gtest/gtest.h>

#include <Eigen/Core>

#include "Core/Util/Math.h"
#include "Scene/Transform.h"
#include "MathTestUtils.h"

TEST(TransformTests, DefaultTransformReturnsIdentity)
{
    Transform t;
    Eigen::Matrix4f expected = Eigen::Matrix4f::Identity();

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, TranslationOnly)
{
    Transform t;
    t.m_Position = {1.0f, 2.0f, 3.0f};

    Eigen::Matrix4f expected = Math::Translate(Eigen::Matrix4f::Identity(), t.m_Position);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, ScaleOnly)
{
    Transform t;
    t.m_Scale = {2.0f, 3.0f, 4.0f};

    Eigen::Matrix4f expected = Math::Scale(Eigen::Matrix4f::Identity(), t.m_Scale);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, RotationOrderMatchesImplementation)
{
    Transform t;
    t.m_RotationEulerDegrees = {30.0f, 45.0f, 60.0f};

    Eigen::Matrix4f rotationX =
        Math::Rotate(Eigen::Matrix4f::Identity(), Math::Radians(30.0f), Eigen::Vector3f(1, 0, 0));
    Eigen::Matrix4f rotationY =
        Math::Rotate(Eigen::Matrix4f::Identity(), Math::Radians(45.0f), Eigen::Vector3f(0, 1, 0));
    Eigen::Matrix4f rotationZ =
        Math::Rotate(Eigen::Matrix4f::Identity(), Math::Radians(60.0f), Eigen::Vector3f(0, 0, 1));

    Eigen::Matrix4f expected = rotationZ * rotationY * rotationX;

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, FullTRSCompositionMatchesImplementation)
{
    Transform t;
    t.m_Position = {1.0f, 2.0f, 3.0f};
    t.m_RotationEulerDegrees = {10.0f, 20.0f, 30.0f};
    t.m_Scale = {2.0f, 2.0f, 2.0f};

    Eigen::Matrix4f translation = Math::Translate(Eigen::Matrix4f::Identity(), t.m_Position);
    Eigen::Matrix4f rotationX =
        Math::Rotate(Eigen::Matrix4f::Identity(), Math::Radians(10.0f), Eigen::Vector3f(1, 0, 0));
    Eigen::Matrix4f rotationY =
        Math::Rotate(Eigen::Matrix4f::Identity(), Math::Radians(20.0f), Eigen::Vector3f(0, 1, 0));
    Eigen::Matrix4f rotationZ =
        Math::Rotate(Eigen::Matrix4f::Identity(), Math::Radians(30.0f), Eigen::Vector3f(0, 0, 1));
    Eigen::Matrix4f rotation = rotationZ * rotationY * rotationX;
    Eigen::Matrix4f scale = Math::Scale(Eigen::Matrix4f::Identity(), t.m_Scale);

    Eigen::Matrix4f expected = translation * rotation * scale;

    ExpectMat4Near(t.GetMatrix(), expected);
}
