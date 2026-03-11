#include <gtest/gtest.h>
#include "graphics/Buffers.h"

TEST(BufferTests, ShaderDataTypeSizeIsCorrect)
{
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float), 4u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float2), 8u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float3), 12u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Float4), 16u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Mat3), 36u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Mat4), 64u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Int), 4u);
    EXPECT_EQ(ShaderDataTypeSize(ShaderDataType::Bool), 1u);
}

TEST(BufferTests, BufferElementComponentCountIsCorrect)
{
    EXPECT_EQ(BufferElement(ShaderDataType::Float, "a").GetComponentCount(), 1u);
    EXPECT_EQ(BufferElement(ShaderDataType::Float2, "a").GetComponentCount(), 2u);
    EXPECT_EQ(BufferElement(ShaderDataType::Float3, "a").GetComponentCount(), 3u);
    EXPECT_EQ(BufferElement(ShaderDataType::Float4, "a").GetComponentCount(), 4u);
    EXPECT_EQ(BufferElement(ShaderDataType::Mat3, "a").GetComponentCount(), 3u);
    EXPECT_EQ(BufferElement(ShaderDataType::Mat4, "a").GetComponentCount(), 4u);
    EXPECT_EQ(BufferElement(ShaderDataType::Int4, "a").GetComponentCount(), 4u);
}

TEST(BufferTests, BufferLayoutCalculatesOffsetsAndStride)
{
    BufferLayout layout = {
        {ShaderDataType::Float3, "a_Position"},
        {ShaderDataType::Float3, "a_Normal"},
        {ShaderDataType::Float2, "a_TexCoord"}};

    const auto &elements = layout.GetElements();

    ASSERT_EQ(elements.size(), 3u);

    EXPECT_EQ(elements[0].Offset, 0u);
    EXPECT_EQ(elements[1].Offset, 12u);
    EXPECT_EQ(elements[2].Offset, 24u);

    EXPECT_EQ(layout.GetStride(), 32u);
}