#include <gtest/gtest.h>

#include "Core/Util/Math.h"
#include "MathTestUtils.h"
#include "Scene/Transform.h"

TEST(TransformTests, DefaultTransformReturnsIdentity)
{
    Transform t;
    Math::Mat4 expected = Math::Mat4::Identity();

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, TranslationOnly)
{
    Transform t;
    t.m_Position = {1.0f, 2.0f, 3.0f};

    Math::Mat4 expected = Math::Translate(Math::Mat4::Identity(), t.m_Position);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, ScaleOnly)
{
    Transform t;
    t.m_Scale = {2.0f, 3.0f, 4.0f};

    Math::Mat4 expected = Math::Scale(Math::Mat4::Identity(), t.m_Scale);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, RotationOrderMatchesImplementation)
{
    Transform t;
    t.m_RotationEulerDegrees = {30.0f, 45.0f, 60.0f};

    Math::Mat4 rotationX = Math::Rotate(Math::Mat4::Identity(), Math::Radians(30.0f), Math::Vec3(1, 0, 0));
    Math::Mat4 rotationY = Math::Rotate(Math::Mat4::Identity(), Math::Radians(45.0f), Math::Vec3(0, 1, 0));
    Math::Mat4 rotationZ = Math::Rotate(Math::Mat4::Identity(), Math::Radians(60.0f), Math::Vec3(0, 0, 1));

    Math::Mat4 expected = rotationZ * rotationY * rotationX;

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, FullTRSCompositionMatchesImplementation)
{
    Transform t;
    t.m_Position = {1.0f, 2.0f, 3.0f};
    t.m_RotationEulerDegrees = {10.0f, 20.0f, 30.0f};
    t.m_Scale = {2.0f, 2.0f, 2.0f};

    Math::Mat4 translation = Math::Translate(Math::Mat4::Identity(), t.m_Position);
    Math::Mat4 rotationX = Math::Rotate(Math::Mat4::Identity(), Math::Radians(10.0f), Math::Vec3(1, 0, 0));
    Math::Mat4 rotationY = Math::Rotate(Math::Mat4::Identity(), Math::Radians(20.0f), Math::Vec3(0, 1, 0));
    Math::Mat4 rotationZ = Math::Rotate(Math::Mat4::Identity(), Math::Radians(30.0f), Math::Vec3(0, 0, 1));
    Math::Mat4 rotation = rotationZ * rotationY * rotationX;
    Math::Mat4 scale = Math::Scale(Math::Mat4::Identity(), t.m_Scale);

    Math::Mat4 expected = translation * rotation * scale;

    ExpectMat4Near(t.GetMatrix(), expected);
}
