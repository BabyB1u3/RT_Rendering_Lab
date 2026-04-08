#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Serialization/BuiltinTraits.h"
#include "Core/Resource/AssetPath.h"

using namespace Serialization;

namespace
{
    enum class Color
    {
        Red,
        Green,
        Blue
    };

    void ExpectVec2Eq(const glm::vec2 &lhs, const glm::vec2 &rhs)
    {
        EXPECT_FLOAT_EQ(lhs.x, rhs.x);
        EXPECT_FLOAT_EQ(lhs.y, rhs.y);
    }

    void ExpectVec3Eq(const glm::vec3 &lhs, const glm::vec3 &rhs)
    {
        EXPECT_FLOAT_EQ(lhs.x, rhs.x);
        EXPECT_FLOAT_EQ(lhs.y, rhs.y);
        EXPECT_FLOAT_EQ(lhs.z, rhs.z);
    }

    void ExpectVec4Eq(const glm::vec4 &lhs, const glm::vec4 &rhs)
    {
        EXPECT_FLOAT_EQ(lhs.x, rhs.x);
        EXPECT_FLOAT_EQ(lhs.y, rhs.y);
        EXPECT_FLOAT_EQ(lhs.z, rhs.z);
        EXPECT_FLOAT_EQ(lhs.w, rhs.w);
    }

    void ExpectMat4Near(const glm::mat4 &lhs, const glm::mat4 &rhs)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
                EXPECT_NEAR(lhs[col][row], rhs[col][row], 1e-5f);
        }
    }
} // namespace

TEST(BuiltinTraitsTests, BoolRoundTrip)
{
    PropertyTree tree;
    Serialize(tree, true);

    bool value = false;
    ASSERT_TRUE(Deserialize(tree, value));
    EXPECT_TRUE(tree.IsBool());
    EXPECT_TRUE(value);

    Serialize(tree, false);
    value = true;
    ASSERT_TRUE(Deserialize(tree, value));
    EXPECT_FALSE(value);
}

TEST(BuiltinTraitsTests, IntRoundTrip)
{
    for (int input : {42, -7, 0})
    {
        PropertyTree tree;
        Serialize(tree, input);

        int output = 123;
        ASSERT_TRUE(Deserialize(tree, output));
        EXPECT_TRUE(tree.IsInt());
        EXPECT_EQ(output, input);
    }
}

TEST(BuiltinTraitsTests, Int64RoundTrip)
{
    const int64_t input = int64_t{1} << 40;
    PropertyTree tree;
    Serialize(tree, input);

    int64_t output = 0;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsInt());
    EXPECT_EQ(output, input);
}

TEST(BuiltinTraitsTests, FloatRoundTrip)
{
    PropertyTree tree;
    Serialize(tree, 3.14f);

    float output = 0.0f;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsFloat());
    EXPECT_FLOAT_EQ(output, 3.14f);
}

TEST(BuiltinTraitsTests, DoubleRoundTrip)
{
    PropertyTree tree;
    Serialize(tree, 3.14159265);

    double output = 0.0;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsFloat());
    EXPECT_DOUBLE_EQ(output, 3.14159265);
}

TEST(BuiltinTraitsTests, StringRoundTrip)
{
    PropertyTree tree;
    Serialize(tree, std::string("RTRLab"));

    std::string output = "fallback";
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(output, "RTRLab");
}

TEST(BuiltinTraitsTests, AssetPathRoundTrip)
{
    const auto input = Resource::AssetPath::TryCreate("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(input.has_value());

    PropertyTree tree;
    Serialize(tree, *input);

    Resource::AssetPath output;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(output.String(), "/Project/Textures/Grassy_Square");
}

TEST(BuiltinTraitsTests, AssetPathDeserializeRejectsAbsoluteFilesystemPathAndLeavesOutputUnchanged)
{
    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    Resource::AssetPath output = *original;

    EXPECT_FALSE(Deserialize(PropertyTree("C:/Users/name/dev/RTRLab/Content/textures/Grassy_Square.jpg"), output));
    EXPECT_EQ(output, *original);
}

TEST(BuiltinTraitsTests, AssetPathDeserializeRejectsSavedDomainAndLeavesOutputUnchanged)
{
    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    Resource::AssetPath output = *original;

    EXPECT_FALSE(Deserialize(PropertyTree("/Saved/Config/Input/DebugCameraControl.json"), output));
    EXPECT_EQ(output, *original);
}

TEST(BuiltinTraitsTests, AssetPathDeserializeRejectsDocumentPathAndLeavesOutputUnchanged)
{
    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    Resource::AssetPath output = *original;

    EXPECT_FALSE(Deserialize(PropertyTree("/Project/Config/input/DebugCameraControl.json"), output));
    EXPECT_EQ(output, *original);
}

TEST(BuiltinTraitsTests, Uint8RoundTrip)
{
    PropertyTree tree;
    Serialize(tree, static_cast<uint8_t>(255));

    uint8_t output = 0;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsInt());
    EXPECT_EQ(output, static_cast<uint8_t>(255));
}

TEST(BuiltinTraitsTests, Uint16RoundTrip)
{
    PropertyTree tree;
    Serialize(tree, static_cast<uint16_t>(60000));

    uint16_t output = 0;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsInt());
    EXPECT_EQ(output, static_cast<uint16_t>(60000));
}

TEST(BuiltinTraitsTests, DeserializeBoolFromIntReturnsFalseAndLeavesOutputUnchanged)
{
    bool value = true;

    EXPECT_FALSE(Deserialize(PropertyTree(1), value));
    EXPECT_TRUE(value);
}

TEST(BuiltinTraitsTests, DeserializeIntFromBoolReturnsFalseAndLeavesOutputUnchanged)
{
    int value = 17;

    EXPECT_FALSE(Deserialize(PropertyTree(true), value));
    EXPECT_EQ(value, 17);
}

TEST(BuiltinTraitsTests, DeserializeIntFromFloatReturnsFalseAndLeavesOutputUnchanged)
{
    int value = 17;

    EXPECT_FALSE(Deserialize(PropertyTree(1.5), value));
    EXPECT_EQ(value, 17);
}

TEST(BuiltinTraitsTests, DeserializeStringFromIntReturnsFalseAndLeavesOutputUnchanged)
{
    std::string value = "sentinel";

    EXPECT_FALSE(Deserialize(PropertyTree(42), value));
    EXPECT_EQ(value, "sentinel");
}

TEST(BuiltinTraitsTests, FloatDeserializesFromIntNode)
{
    float value = 0.0f;

    ASSERT_TRUE(Deserialize(PropertyTree(7), value));
    EXPECT_FLOAT_EQ(value, 7.0f);
}

TEST(BuiltinTraitsTests, DoubleDeserializesFromIntNode)
{
    double value = 0.0;

    ASSERT_TRUE(Deserialize(PropertyTree(10), value));
    EXPECT_DOUBLE_EQ(value, 10.0);
}

TEST(BuiltinTraitsTests, FloatDeserializesFromFloatNode)
{
    float value = 0.0f;

    ASSERT_TRUE(Deserialize(PropertyTree(2.5f), value));
    EXPECT_FLOAT_EQ(value, 2.5f);
}

TEST(BuiltinTraitsTests, Uint8DeserializeRejectsNegativeAndOutOfRangeValues)
{
    uint8_t value = 11;

    EXPECT_FALSE(Deserialize(PropertyTree(-1), value));
    EXPECT_EQ(value, static_cast<uint8_t>(11));
    EXPECT_FALSE(Deserialize(PropertyTree(256), value));
    EXPECT_EQ(value, static_cast<uint8_t>(11));
}

TEST(BuiltinTraitsTests, Uint16DeserializeRejectsNegativeAndOutOfRangeValues)
{
    uint16_t value = 22;

    EXPECT_FALSE(Deserialize(PropertyTree(-1), value));
    EXPECT_EQ(value, static_cast<uint16_t>(22));
    EXPECT_FALSE(Deserialize(PropertyTree(65536), value));
    EXPECT_EQ(value, static_cast<uint16_t>(22));
}

TEST(BuiltinTraitsTests, EnumSerializesToStringToken)
{
    PropertyTree tree;
    Serialize(tree, Color::Green);

    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(tree.AsString(), "Green");
}

TEST(BuiltinTraitsTests, EnumDeserializesFromValidString)
{
    Color value = Color::Red;

    ASSERT_TRUE(Deserialize(PropertyTree("Blue"), value));
    EXPECT_EQ(value, Color::Blue);
}

TEST(BuiltinTraitsTests, EnumDeserializeUnknownStringReturnsFalseAndLeavesOutputUnchanged)
{
    Color value = Color::Green;

    EXPECT_FALSE(Deserialize(PropertyTree("Purple"), value));
    EXPECT_EQ(value, Color::Green);
}

TEST(BuiltinTraitsTests, EnumDeserializeFromNonStringReturnsFalseAndLeavesOutputUnchanged)
{
    Color value = Color::Green;

    EXPECT_FALSE(Deserialize(PropertyTree(42), value));
    EXPECT_EQ(value, Color::Green);
}

TEST(BuiltinTraitsTests, Vec2RoundTrip)
{
    const glm::vec2 input(1.0f, -2.0f);
    PropertyTree tree;
    Serialize(tree, input);

    glm::vec2 output(0.0f);
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsArray());
    EXPECT_EQ(tree.Size(), 2u);
    ExpectVec2Eq(output, input);
}

TEST(BuiltinTraitsTests, Vec3RoundTrip)
{
    const glm::vec3 input(1.0f, -2.0f, 3.5f);
    PropertyTree tree;
    Serialize(tree, input);

    glm::vec3 output(0.0f);
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsArray());
    EXPECT_EQ(tree.Size(), 3u);
    ExpectVec3Eq(output, input);
}

TEST(BuiltinTraitsTests, Vec4RoundTrip)
{
    const glm::vec4 input(1.0f, -2.0f, 3.5f, 4.25f);
    PropertyTree tree;
    Serialize(tree, input);

    glm::vec4 output(0.0f);
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsArray());
    EXPECT_EQ(tree.Size(), 4u);
    ExpectVec4Eq(output, input);
}

TEST(BuiltinTraitsTests, Mat4RoundTripUsesColumnMajorLayout)
{
    const glm::mat4 input(
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f);

    PropertyTree tree;
    Serialize(tree, input);

    ASSERT_TRUE(tree.IsArray());
    ASSERT_EQ(tree.Size(), 16u);
    EXPECT_FLOAT_EQ(static_cast<float>(tree[0].AsFloat()), input[0][0]);
    EXPECT_FLOAT_EQ(static_cast<float>(tree[1].AsFloat()), input[0][1]);
    EXPECT_FLOAT_EQ(static_cast<float>(tree[4].AsFloat()), input[1][0]);
    EXPECT_FLOAT_EQ(static_cast<float>(tree[15].AsFloat()), input[3][3]);

    glm::mat4 output(0.0f);
    ASSERT_TRUE(Deserialize(tree, output));
    ExpectMat4Near(output, input);
}

TEST(BuiltinTraitsTests, Vec3DeserializeFromTooShortArrayReturnsFalseAndLeavesOutputUnchanged)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1.0f), PropertyTree(2.0f)});
    glm::vec3 value(9.0f, 8.0f, 7.0f);

    EXPECT_FALSE(Deserialize(tree, value));
    ExpectVec3Eq(value, glm::vec3(9.0f, 8.0f, 7.0f));
}

TEST(BuiltinTraitsTests, Vec3DeserializeFromTooLongArrayReturnsFalseAndLeavesOutputUnchanged)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1.0f), PropertyTree(2.0f), PropertyTree(3.0f), PropertyTree(4.0f)});
    glm::vec3 value(9.0f, 8.0f, 7.0f);

    EXPECT_FALSE(Deserialize(tree, value));
    ExpectVec3Eq(value, glm::vec3(9.0f, 8.0f, 7.0f));
}

TEST(BuiltinTraitsTests, Mat4DeserializeFromTooShortArrayReturnsFalseAndLeavesOutputUnchanged)
{
    PropertyTree::Array arr(15, PropertyTree(1.0f));
    PropertyTree tree(std::move(arr));
    glm::mat4 value(2.0f);
    const glm::mat4 original = value;

    EXPECT_FALSE(Deserialize(tree, value));
    ExpectMat4Near(value, original);
}

TEST(BuiltinTraitsTests, Mat4DeserializeFromTooLongArrayReturnsFalseAndLeavesOutputUnchanged)
{
    PropertyTree::Array arr(17, PropertyTree(1.0f));
    PropertyTree tree(std::move(arr));
    glm::mat4 value(2.0f);
    const glm::mat4 original = value;

    EXPECT_FALSE(Deserialize(tree, value));
    ExpectMat4Near(value, original);
}

TEST(BuiltinTraitsTests, VectorIntRoundTrip)
{
    const std::vector<int> input{1, 2, 3};
    PropertyTree tree;
    Serialize(tree, input);

    std::vector<int> output;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsArray());
    EXPECT_EQ(output, input);
}

TEST(BuiltinTraitsTests, VectorStringRoundTrip)
{
    const std::vector<std::string> input{"alpha", "beta"};
    PropertyTree tree;
    Serialize(tree, input);

    std::vector<std::string> output;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_EQ(output, input);
}

TEST(BuiltinTraitsTests, VectorEmptyRoundTrip)
{
    const std::vector<int> input{};
    PropertyTree tree;
    Serialize(tree, input);

    std::vector<int> output{1, 2, 3};
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsArray());
    EXPECT_TRUE(output.empty());
}

TEST(BuiltinTraitsTests, VectorDeserializeFromNonArrayReturnsFalseAndLeavesOutputUnchanged)
{
    std::vector<int> output{7, 8};

    EXPECT_FALSE(Deserialize(PropertyTree::Object{}, output));
    EXPECT_EQ(output, (std::vector<int>{7, 8}));
}

TEST(BuiltinTraitsTests, VectorDeserializePartialFailureLeavesOutputUnchanged)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree("bad"), PropertyTree(3)});
    std::vector<int> output{9, 9};

    EXPECT_FALSE(Deserialize(tree, output));
    EXPECT_EQ(output, (std::vector<int>{9, 9}));
}

TEST(BuiltinTraitsTests, MapIntRoundTrip)
{
    const std::unordered_map<std::string, int> input{{"a", 1}, {"b", 2}};
    PropertyTree tree;
    Serialize(tree, input);

    std::unordered_map<std::string, int> output;
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsObject());
    EXPECT_EQ(output, input);
}

TEST(BuiltinTraitsTests, MapEmptyRoundTrip)
{
    const std::unordered_map<std::string, int> input{};
    PropertyTree tree;
    Serialize(tree, input);

    std::unordered_map<std::string, int> output{{"x", 5}};
    ASSERT_TRUE(Deserialize(tree, output));
    EXPECT_TRUE(tree.IsObject());
    EXPECT_TRUE(output.empty());
}

TEST(BuiltinTraitsTests, MapDeserializeFromNonObjectReturnsFalseAndLeavesOutputUnchanged)
{
    std::unordered_map<std::string, int> output{{"sentinel", 7}};

    EXPECT_FALSE(Deserialize(PropertyTree::Array{}, output));
    EXPECT_EQ(output, (std::unordered_map<std::string, int>{{"sentinel", 7}}));
}

TEST(BuiltinTraitsTests, MapDeserializePartialFailureLeavesOutputUnchanged)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["good"] = PropertyTree(1);
    tree["bad"] = PropertyTree("oops");

    std::unordered_map<std::string, int> output{{"sentinel", 7}};
    EXPECT_FALSE(Deserialize(tree, output));
    EXPECT_EQ(output, (std::unordered_map<std::string, int>{{"sentinel", 7}}));
}

TEST(BuiltinTraitsTests, OptionalWithValueRoundTrip)
{
    const std::optional<int> input = 42;
    PropertyTree tree;
    Serialize(tree, input);

    std::optional<int> output;
    ASSERT_TRUE(Deserialize(tree, output));
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, 42);
}

TEST(BuiltinTraitsTests, OptionalNulloptSerializesToNull)
{
    PropertyTree tree;
    Serialize(tree, std::optional<int>{});

    EXPECT_TRUE(tree.IsNull());
}

TEST(BuiltinTraitsTests, OptionalDeserializeFromNullGivesNullopt)
{
    std::optional<int> output = 9;

    ASSERT_TRUE(Deserialize(PropertyTree(nullptr), output));
    EXPECT_FALSE(output.has_value());
}

TEST(BuiltinTraitsTests, OptionalDeserializeTypeMismatchReturnsFalseAndLeavesOutputUnchanged)
{
    std::optional<int> output = 13;

    EXPECT_FALSE(Deserialize(PropertyTree("bad"), output));
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, 13);
}

TEST(BuiltinTraitsTests, NestedBuiltinContainersRoundTrip)
{
    const std::optional<std::vector<int>> input = std::vector<int>{4, 5, 6};
    PropertyTree tree;
    Serialize(tree, input);

    std::optional<std::vector<int>> output;
    ASSERT_TRUE(Deserialize(tree, output));
    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(*output, (std::vector<int>{4, 5, 6}));
}
