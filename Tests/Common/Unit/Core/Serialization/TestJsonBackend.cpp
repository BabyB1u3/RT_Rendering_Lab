#include <gtest/gtest.h>

#include <string>

#include "Core/Serialization/JsonBackend.h"

using Serialization::JsonBackend;
using Serialization::PropertyTree;

namespace
{
void ExpectTreesEqual(const PropertyTree& actual, const PropertyTree& expected)
{
    ASSERT_EQ(actual.IsNull(), expected.IsNull());
    ASSERT_EQ(actual.IsBool(), expected.IsBool());
    ASSERT_EQ(actual.IsInt(), expected.IsInt());
    ASSERT_EQ(actual.IsFloat(), expected.IsFloat());
    ASSERT_EQ(actual.IsString(), expected.IsString());
    ASSERT_EQ(actual.IsArray(), expected.IsArray());
    ASSERT_EQ(actual.IsObject(), expected.IsObject());

    if (expected.IsNull())
        return;
    if (expected.IsBool())
    {
        EXPECT_EQ(actual.AsBool(), expected.AsBool());
        return;
    }
    if (expected.IsInt())
    {
        EXPECT_EQ(actual.AsInt(), expected.AsInt());
        return;
    }
    if (expected.IsFloat())
    {
        EXPECT_DOUBLE_EQ(actual.AsFloat(), expected.AsFloat());
        return;
    }
    if (expected.IsString())
    {
        EXPECT_EQ(actual.AsString(), expected.AsString());
        return;
    }
    if (expected.IsArray())
    {
        ASSERT_EQ(actual.Size(), expected.Size());
        for (size_t i = 0; i < expected.Size(); ++i)
            ExpectTreesEqual(actual[i], expected[i]);
        return;
    }

    ASSERT_TRUE(expected.IsObject());
    ASSERT_EQ(actual.Size(), expected.Size());
    for (const auto& [key, child] : expected.AsObject())
    {
        ASSERT_TRUE(actual.Contains(key));
        ExpectTreesEqual(actual[key], child);
    }
}
} // namespace

TEST(JsonBackendTests, WriteNullProducesNullLiteral)
{
    JsonBackend backend;

    EXPECT_EQ(backend.WriteToString(PropertyTree(nullptr)), "null");
}

TEST(JsonBackendTests, WriteBoolProducesBoolLiteral)
{
    JsonBackend backend;

    EXPECT_EQ(backend.WriteToString(PropertyTree(true)), "true");
}

TEST(JsonBackendTests, WriteIntProducesIntLiteral)
{
    JsonBackend backend;

    EXPECT_EQ(backend.WriteToString(PropertyTree(42)), "42");
}

TEST(JsonBackendTests, WriteFloatProducesNumericLiteral)
{
    JsonBackend backend;

    EXPECT_EQ(backend.WriteToString(PropertyTree(3.5)), "3.5");
}

TEST(JsonBackendTests, WriteStringProducesQuotedEscapedString)
{
    JsonBackend backend;
    PropertyTree tree("line \"quote\"\npath\\file");

    EXPECT_EQ(backend.WriteToString(tree), "\"line \\\"quote\\\"\\npath\\\\file\"");
}

TEST(JsonBackendTests, WriteArrayProducesJsonArray)
{
    JsonBackend backend;
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree(2)});

    EXPECT_EQ(backend.WriteToString(tree), "[\n  1,\n  2\n]");
}

TEST(JsonBackendTests, WriteObjectProducesJsonObject)
{
    JsonBackend backend;
    PropertyTree tree = PropertyTree::Object{{"k", PropertyTree(1)}};

    EXPECT_EQ(backend.WriteToString(tree), "{\n  \"k\": 1\n}");
}

TEST(JsonBackendTests, WriteNestedStructureIsCorrect)
{
    JsonBackend backend;
    PropertyTree tree = PropertyTree::Object{};
    tree["items"] = PropertyTree::Array{PropertyTree(1), PropertyTree(2)};
    tree["nested"] = PropertyTree::Object{{"flag", PropertyTree(true)}};

    EXPECT_EQ(backend.WriteToString(tree),
              "{\n  \"items\": [\n    1,\n    2\n  ],\n  \"nested\": {\n    \"flag\": true\n  }\n}");
}

TEST(JsonBackendTests, DefaultIndentUsesTwoSpacePrettyPrint)
{
    JsonBackend backend;
    PropertyTree tree = PropertyTree::Object{{"value", PropertyTree(1)}};

    EXPECT_NE(backend.WriteToString(tree).find("\n  \"value\""), std::string::npos);
}

TEST(JsonBackendTests, IndentZeroUsesPrettyPrintWithoutIndentSpaces)
{
    JsonBackend backend(0);
    PropertyTree tree = PropertyTree::Object{{"value", PropertyTree(1)}};

    EXPECT_EQ(backend.WriteToString(tree), "{\n\"value\": 1\n}");
}

TEST(JsonBackendTests, ReadNullJsonGivesNullTree)
{
    JsonBackend backend;
    PropertyTree tree("unchanged");

    ASSERT_TRUE(backend.ReadFromString("null", tree));
    EXPECT_TRUE(tree.IsNull());
}

TEST(JsonBackendTests, ReadBoolJsonGivesBoolTree)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(backend.ReadFromString("true", tree));
    EXPECT_TRUE(tree.IsBool());
    EXPECT_TRUE(tree.AsBool());
}

TEST(JsonBackendTests, ReadIntJsonGivesIntTree)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(backend.ReadFromString("42", tree));
    EXPECT_TRUE(tree.IsInt());
    EXPECT_EQ(tree.AsInt(), 42);
}

TEST(JsonBackendTests, ReadFloatJsonGivesFloatTree)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(backend.ReadFromString("3.14", tree));
    EXPECT_TRUE(tree.IsFloat());
    EXPECT_DOUBLE_EQ(tree.AsFloat(), 3.14);
}

TEST(JsonBackendTests, ReadStringJsonGivesStringTree)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(backend.ReadFromString("\"hello\"", tree));
    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(tree.AsString(), "hello");
}

TEST(JsonBackendTests, ReadArrayJsonGivesArrayTree)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(backend.ReadFromString("[1,2,3]", tree));
    EXPECT_TRUE(tree.IsArray());
    ASSERT_EQ(tree.Size(), 3u);
    EXPECT_EQ(tree[0].AsInt(), 1);
    EXPECT_EQ(tree[2].AsInt(), 3);
}

TEST(JsonBackendTests, ReadObjectJsonGivesObjectTree)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(backend.ReadFromString("{\"x\":1}", tree));
    EXPECT_TRUE(tree.IsObject());
    ASSERT_TRUE(tree.Contains("x"));
    EXPECT_EQ(tree["x"].AsInt(), 1);
}

TEST(JsonBackendTests, ReadNestedStructureIsCorrect)
{
    JsonBackend backend;
    PropertyTree tree;

    ASSERT_TRUE(
        backend.ReadFromString("{\"array\":[1,2.5,null],\"nested\":{\"flag\":true,\"name\":\"RTRLab\"}}", tree));

    ASSERT_TRUE(tree.IsObject());
    ASSERT_TRUE(tree.Contains("array"));
    ASSERT_TRUE(tree.Contains("nested"));
    ASSERT_TRUE(tree["array"].IsArray());
    EXPECT_EQ(tree["array"][0].AsInt(), 1);
    EXPECT_DOUBLE_EQ(tree["array"][1].AsFloat(), 2.5);
    EXPECT_TRUE(tree["array"][2].IsNull());
    EXPECT_TRUE(tree["nested"]["flag"].AsBool());
    EXPECT_EQ(tree["nested"]["name"].AsString(), "RTRLab");
}

TEST(JsonBackendTests, ReadTooLargeUnsignedIntegerReturnsFalseAndLeavesTreeUnchanged)
{
    JsonBackend backend;
    PropertyTree tree("sentinel");

    EXPECT_FALSE(backend.ReadFromString("9223372036854775808", tree));
    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(tree.AsString(), "sentinel");
}

TEST(JsonBackendTests, RoundTripBool)
{
    JsonBackend backend;
    PropertyTree input(true);
    PropertyTree output;

    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, RoundTripInt)
{
    JsonBackend backend;
    PropertyTree input(-42);
    PropertyTree output;

    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, RoundTripFloat)
{
    JsonBackend backend;
    PropertyTree input(3.1415926535);
    PropertyTree output;

    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, RoundTripEscapedString)
{
    JsonBackend backend;
    PropertyTree input("line \"quote\"\nslash\\path");
    PropertyTree output;

    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, RoundTripArray)
{
    JsonBackend backend;
    PropertyTree input(PropertyTree::Array{PropertyTree(1), PropertyTree("two"), PropertyTree(false)});
    PropertyTree output;

    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, RoundTripNestedObject)
{
    JsonBackend backend;
    PropertyTree input = PropertyTree::Object{};
    input["name"] = PropertyTree("RTRLab");
    input["values"] = PropertyTree::Array{PropertyTree(1), PropertyTree(2.5), PropertyTree(nullptr)};
    input["nested"] = PropertyTree::Object{{"enabled", PropertyTree(true)}};

    PropertyTree output;
    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, RoundTripNull)
{
    JsonBackend backend;
    PropertyTree input(nullptr);
    PropertyTree output("sentinel");

    ASSERT_TRUE(backend.ReadFromString(backend.WriteToString(input), output));
    ExpectTreesEqual(output, input);
}

TEST(JsonBackendTests, MalformedJsonReturnsFalse)
{
    JsonBackend backend;
    PropertyTree tree;

    EXPECT_FALSE(backend.ReadFromString("{invalid", tree));
}

TEST(JsonBackendTests, EmptyStringReturnsFalse)
{
    JsonBackend backend;
    PropertyTree tree;

    EXPECT_FALSE(backend.ReadFromString("", tree));
}

TEST(JsonBackendTests, TreeUnchangedOnParseFailure)
{
    JsonBackend backend;
    PropertyTree tree("sentinel");

    EXPECT_FALSE(backend.ReadFromString("{ bad json", tree));
    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(tree.AsString(), "sentinel");
}
