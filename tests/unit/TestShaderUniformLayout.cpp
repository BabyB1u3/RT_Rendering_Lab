#include <gtest/gtest.h>

#include <cstring>
#include <glm/glm.hpp>

#include "graphics/ShaderUniformLayout.h"

TEST(ShaderUniformLayoutTests, ValueTypeSizesMatchPackingExpectations)
{
    EXPECT_EQ(GetShaderUniformValueTypeSize(ShaderUniformValueType::Bool), 4u);
    EXPECT_EQ(GetShaderUniformValueTypeSize(ShaderUniformValueType::Float3), 12u);
    EXPECT_EQ(GetShaderUniformValueTypeSize(ShaderUniformValueType::Mat4), 64u);
}

TEST(ShaderUniformLayoutTests, PackedUniformBlockWritesFieldsAtReflectedOffsets)
{
    ShaderUniformBlockLayout layout("TestBlock", 0, 32);
    ASSERT_TRUE(layout.AddField({"u_First", 0, 4, ShaderUniformValueType::Float}));
    ASSERT_TRUE(layout.AddField({"u_Flag", 16, 4, ShaderUniformValueType::Bool}));

    PackedUniformBlock block(layout);

    const float first = 3.5f;
    ASSERT_TRUE(block.Write("u_First", first));
    ASSERT_TRUE(block.Write("u_Flag", true));

    const auto *bytes = static_cast<const std::byte *>(block.Data());
    float storedFirst = 0.0f;
    int32_t storedFlag = 0;
    std::memcpy(&storedFirst, bytes + 0, sizeof(storedFirst));
    std::memcpy(&storedFlag, bytes + 16, sizeof(storedFlag));

    EXPECT_EQ(block.Size(), 32u);
    EXPECT_EQ(storedFirst, 3.5f);
    EXPECT_EQ(storedFlag, 1);
}

TEST(ShaderUniformLayoutTests, PackedUniformBlockRejectsWrongSizedWrites)
{
    ShaderUniformBlockLayout layout("TestBlock", 0, 16);
    ASSERT_TRUE(layout.AddField({"u_Value", 0, 4, ShaderUniformValueType::Float}));

    PackedUniformBlock block(layout);
    const glm::vec2 wrongSizeValue(1.0f, 2.0f);

    EXPECT_FALSE(block.Write("u_Value", wrongSizeValue));
    EXPECT_FALSE(block.GetLastError().empty());
}

TEST(ShaderUniformLayoutTests, PackedUniformBlockAcceptsLogicalFloat3WriteIntoPaddedField)
{
    ShaderUniformBlockLayout layout("TestBlock", 0, 32);
    ASSERT_TRUE(layout.AddField({"u_Vector", 0, 16, ShaderUniformValueType::Float3}));

    PackedUniformBlock block(layout);
    const glm::vec3 value(1.0f, 2.0f, 3.0f);

    ASSERT_TRUE(block.Write("u_Vector", value));

    const auto *bytes = static_cast<const std::byte *>(block.Data());
    glm::vec3 storedValue(0.0f);
    float padding = 123.0f;
    std::memcpy(&storedValue, bytes + 0, sizeof(storedValue));
    std::memcpy(&padding, bytes + 12, sizeof(padding));

    EXPECT_EQ(storedValue, value);
    EXPECT_EQ(padding, 0.0f);
}

TEST(ShaderUniformLayoutTests, PackedUniformBlockWritesBoolUsingReflectedFieldSize)
{
    ShaderUniformBlockLayout layout("TestBlock", 0, 8);
    ASSERT_TRUE(layout.AddField({"u_Flag", 0, 1, ShaderUniformValueType::Bool}));

    PackedUniformBlock block(layout);
    ASSERT_TRUE(block.Write("u_Flag", true));

    const auto *bytes = static_cast<const std::byte *>(block.Data());
    uint8_t storedValue = 0;
    std::memcpy(&storedValue, bytes + 0, sizeof(storedValue));

    EXPECT_EQ(storedValue, 1u);
}

TEST(ShaderUniformLayoutTests, WriteRequiredSucceedsAndWritesData)
{
    ShaderUniformBlockLayout layout("TestBlock", 0, 8);
    ASSERT_TRUE(layout.AddField({"u_Value", 0, 4, ShaderUniformValueType::Float}));

    PackedUniformBlock block(layout);
    block.WriteRequired("u_Value", 7.5f);

    const auto *bytes = static_cast<const std::byte *>(block.Data());
    float stored = 0.0f;
    std::memcpy(&stored, bytes, sizeof(stored));
    EXPECT_EQ(stored, 7.5f);
}

TEST(ShaderUniformLayoutTests, WriteReturnsFalseAndSetsErrorForUnknownField)
{
    ShaderUniformBlockLayout layout("TestBlock", 0, 8);
    ASSERT_TRUE(layout.AddField({"u_Real", 0, 4, ShaderUniformValueType::Float}));

    PackedUniformBlock block(layout);
    EXPECT_FALSE(block.Write("u_Missing", 1.0f));
    EXPECT_FALSE(block.GetLastError().empty());
}

// --- NormalizeGLUniformFieldName ---

TEST(NormalizeGLUniformFieldNameTests, LeafNameIsUnchanged)
{
    EXPECT_EQ(NormalizeGLUniformFieldName("u_CameraPosition"), "u_CameraPosition");
}

TEST(NormalizeGLUniformFieldNameTests, StripBlockPrefix)
{
    EXPECT_EQ(NormalizeGLUniformFieldName("PerFrameBlock.u_ViewProjection"), "u_ViewProjection");
}

TEST(NormalizeGLUniformFieldNameTests, StripArraySuffix)
{
    EXPECT_EQ(NormalizeGLUniformFieldName("u_Lights[0]"), "u_Lights");
}

TEST(NormalizeGLUniformFieldNameTests, StripSlangNumericSuffix)
{
    EXPECT_EQ(NormalizeGLUniformFieldName("u_CameraPosition_0"), "u_CameraPosition");
}

TEST(NormalizeGLUniformFieldNameTests, StripBlockPrefixAndNumericSuffix)
{
    EXPECT_EQ(NormalizeGLUniformFieldName("GlobalBlock.u_LightDir_0"), "u_LightDir");
}

TEST(NormalizeGLUniformFieldNameTests, DoesNotStripNonNumericSuffix)
{
    EXPECT_EQ(NormalizeGLUniformFieldName("u_Value_final"), "u_Value_final");
}

TEST(NormalizeGLUniformFieldNameTests, UseLastDotForNestedPath)
{
    // Single-level nesting: last dot wins.
    EXPECT_EQ(NormalizeGLUniformFieldName("Outer.Inner.u_Field"), "u_Field");
}
