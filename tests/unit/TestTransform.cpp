#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

#include "scene/Transform.h"
#include "MathTestUtils.h"

TEST(TransformTests, DefaultTransformReturnsIdentity)
{
    Transform t;
    glm::mat4 expected(1.0f);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, TranslationOnly)
{
    Transform t;
    t.Position = {1.0f, 2.0f, 3.0f};

    glm::mat4 expected = glm::translate(glm::mat4(1.0f), t.Position);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, ScaleOnly)
{
    Transform t;
    t.Scale = {2.0f, 3.0f, 4.0f};

    glm::mat4 expected = glm::scale(glm::mat4(1.0f), t.Scale);

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, RotationOrderMatchesImplementation)
{
    Transform t;
    t.RotationEulerDegrees = {30.0f, 45.0f, 60.0f};

    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1, 0, 0));
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), glm::radians(60.0f), glm::vec3(0, 0, 1));

    glm::mat4 expected = rotationZ * rotationY * rotationX;

    ExpectMat4Near(t.GetMatrix(), expected);
}

TEST(TransformTests, FullTRSCompositionMatchesImplementation)
{
    Transform t;
    t.Position = {1.0f, 2.0f, 3.0f};
    t.RotationEulerDegrees = {10.0f, 20.0f, 30.0f};
    t.Scale = {2.0f, 2.0f, 2.0f};

    glm::mat4 translation = glm::translate(glm::mat4(1.0f), t.Position);
    glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), glm::radians(10.0f), glm::vec3(1, 0, 0));
    glm::mat4 rotationY = glm::rotate(glm::mat4(1.0f), glm::radians(20.0f), glm::vec3(0, 1, 0));
    glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0, 0, 1));
    glm::mat4 rotation = rotationZ * rotationY * rotationX;
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), t.Scale);

    glm::mat4 expected = translation * rotation * scale;

    ExpectMat4Near(t.GetMatrix(), expected);
}