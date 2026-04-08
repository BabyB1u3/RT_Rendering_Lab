#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "Core/Serialization/PropertyTree.h"

using Serialization::PropertyTree;

TEST(PropertyTreeTests, DefaultConstructedIsNull)
{
    PropertyTree tree;

    EXPECT_TRUE(tree.IsNull());
    EXPECT_FALSE(tree.IsBool());
    EXPECT_FALSE(tree.IsInt());
    EXPECT_FALSE(tree.IsFloat());
    EXPECT_FALSE(tree.IsString());
    EXPECT_FALSE(tree.IsArray());
    EXPECT_FALSE(tree.IsObject());
    EXPECT_FALSE(tree.IsNumber());
}

TEST(PropertyTreeTests, NullptrConstructedIsNull)
{
    PropertyTree tree(nullptr);

    EXPECT_TRUE(tree.IsNull());
    EXPECT_FALSE(tree.IsNumber());
}

TEST(PropertyTreeTests, BoolConstructedIsBool)
{
    PropertyTree tree(true);

    EXPECT_TRUE(tree.IsBool());
    EXPECT_FALSE(tree.IsNumber());
}

TEST(PropertyTreeTests, IntConstructedIsInt)
{
    PropertyTree tree(42);

    EXPECT_TRUE(tree.IsInt());
    EXPECT_TRUE(tree.IsNumber());
    EXPECT_EQ(tree.AsInt(), 42);
}

TEST(PropertyTreeTests, Int64ConstructedIsInt)
{
    const int64_t value = int64_t{1} << 40;
    PropertyTree tree(value);

    EXPECT_TRUE(tree.IsInt());
    EXPECT_TRUE(tree.IsNumber());
    EXPECT_EQ(tree.AsInt(), value);
}

TEST(PropertyTreeTests, FloatConstructedIsFloat)
{
    PropertyTree tree(1.5f);

    EXPECT_TRUE(tree.IsFloat());
    EXPECT_TRUE(tree.IsNumber());
    EXPECT_DOUBLE_EQ(tree.AsFloat(), 1.5);
}

TEST(PropertyTreeTests, DoubleConstructedIsFloat)
{
    PropertyTree tree(3.14);

    EXPECT_TRUE(tree.IsFloat());
    EXPECT_TRUE(tree.IsNumber());
    EXPECT_DOUBLE_EQ(tree.AsFloat(), 3.14);
}

TEST(PropertyTreeTests, CStringConstructedIsString)
{
    PropertyTree tree("hello");

    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(tree.AsString(), "hello");
}

TEST(PropertyTreeTests, StdStringConstructedIsString)
{
    PropertyTree tree(std::string("hi"));

    EXPECT_TRUE(tree.IsString());
    EXPECT_EQ(tree.AsString(), "hi");
}

TEST(PropertyTreeTests, ArrayConstructedIsArray)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree(2)});

    EXPECT_TRUE(tree.IsArray());
    EXPECT_EQ(tree.Size(), 2u);
}

TEST(PropertyTreeTests, ObjectConstructedIsObject)
{
    PropertyTree tree(PropertyTree::Object{{"name", PropertyTree("RTRLab")}});

    EXPECT_TRUE(tree.IsObject());
    EXPECT_EQ(tree.Size(), 1u);
}

TEST(PropertyTreeTests, IsNumberTrueOnlyForIntAndFloat)
{
    EXPECT_TRUE(PropertyTree(7).IsNumber());
    EXPECT_TRUE(PropertyTree(2.5).IsNumber());
    EXPECT_FALSE(PropertyTree(false).IsNumber());
    EXPECT_FALSE(PropertyTree("text").IsNumber());
    EXPECT_FALSE(PropertyTree().IsNumber());
}

TEST(PropertyTreeTests, AsBoolReturnsStoredValue)
{
    EXPECT_FALSE(PropertyTree(false).AsBool());
    EXPECT_TRUE(PropertyTree(true).AsBool());
}

TEST(PropertyTreeTests, AsFloatPromotesInt)
{
    PropertyTree tree(7);

    EXPECT_DOUBLE_EQ(tree.AsFloat(), 7.0);
}

TEST(PropertyTreeTests, AsStringReturnsStoredString)
{
    PropertyTree tree("abc");

    EXPECT_EQ(tree.AsString(), "abc");
}

TEST(PropertyTreeTests, AsArrayReturnsStoredArrayAndMutableAccessAllowsModification)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree(2)});

    EXPECT_EQ(tree.AsArray().size(), 2u);

    tree.AsArray().push_back(PropertyTree(3));

    EXPECT_EQ(tree.Size(), 3u);
    EXPECT_EQ(tree[2].AsInt(), 3);
}

TEST(PropertyTreeTests, AsObjectReturnsStoredObjectAndMutableAccessAllowsModification)
{
    PropertyTree tree(PropertyTree::Object{{"name", PropertyTree("RTRLab")}});

    EXPECT_EQ(tree.AsObject().size(), 1u);

    tree.AsObject()["version"] = PropertyTree(1);

    EXPECT_EQ(tree.Size(), 2u);
    EXPECT_TRUE(tree.Contains("version"));
    EXPECT_EQ(tree["version"].AsInt(), 1);
}

TEST(PropertyTreeTests, ContainsReportsPresenceAndAbsence)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["enabled"] = PropertyTree(true);

    EXPECT_TRUE(tree.Contains("enabled"));
    EXPECT_FALSE(tree.Contains("missing"));
}

TEST(PropertyTreeTests, ContainsReturnsFalseForNonObject)
{
    EXPECT_FALSE(PropertyTree(7).Contains("value"));
    EXPECT_FALSE(PropertyTree(PropertyTree::Array{}).Contains("value"));
    EXPECT_FALSE(PropertyTree().Contains("value"));
}

TEST(PropertyTreeTests, ConstObjectSubscriptReturnsCorrectValue)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["count"] = PropertyTree(7);
    const PropertyTree &constTree = tree;

    EXPECT_EQ(constTree["count"].AsInt(), 7);
}

TEST(PropertyTreeTests, MutableObjectSubscriptCreatesEntry)
{
    PropertyTree tree = PropertyTree::Object{};

    tree["new"] = PropertyTree("value");

    EXPECT_TRUE(tree.Contains("new"));
    EXPECT_EQ(tree["new"].AsString(), "value");
}

TEST(PropertyTreeTests, MutableObjectSubscriptOnNullAutoPromotesToObject)
{
    PropertyTree tree;

    tree["answer"] = PropertyTree(42);

    EXPECT_TRUE(tree.IsObject());
    EXPECT_TRUE(tree.Contains("answer"));
    EXPECT_EQ(tree["answer"].AsInt(), 42);
}

TEST(PropertyTreeTests, NestedObjectWritesWorkThroughMutableIndexing)
{
    PropertyTree tree;

    tree["render"]["samples"] = PropertyTree(8);

    EXPECT_TRUE(tree.IsObject());
    EXPECT_TRUE(tree["render"].IsObject());
    EXPECT_EQ(tree["render"]["samples"].AsInt(), 8);
}

TEST(PropertyTreeTests, SizeReturnsKeyCountForObject)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["a"] = PropertyTree(1);
    tree["b"] = PropertyTree(2);

    EXPECT_EQ(tree.Size(), 2u);
}

TEST(PropertyTreeTests, ArraySubscriptReturnsCorrectElement)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree(2)});

    EXPECT_EQ(tree[0].AsInt(), 1);
    EXPECT_EQ(tree[1].AsInt(), 2);
}

TEST(PropertyTreeTests, MutableArraySubscriptAllowsWrite)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree(2)});

    tree[0] = PropertyTree(99);

    EXPECT_EQ(tree[0].AsInt(), 99);
    EXPECT_EQ(tree[1].AsInt(), 2);
}

TEST(PropertyTreeTests, SizeReturnsElementCountForArray)
{
    PropertyTree tree(PropertyTree::Array{PropertyTree(1), PropertyTree(2), PropertyTree(3)});

    EXPECT_EQ(tree.Size(), 3u);
}

TEST(PropertyTreeTests, SizeReturnsZeroForScalars)
{
    EXPECT_EQ(PropertyTree(7).Size(), 0u);
    EXPECT_EQ(PropertyTree(false).Size(), 0u);
    EXPECT_EQ(PropertyTree("name").Size(), 0u);
}

TEST(PropertyTreeTests, GetOrReturnsStoredValueWhenTypeMatches)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["enabled"] = PropertyTree(true);
    tree["count"] = PropertyTree(7);
    tree["name"] = PropertyTree("RTRLab");

    EXPECT_TRUE(tree.GetOr<bool>("enabled", false));
    EXPECT_EQ(tree.GetOr<int>("count", 0), 7);
    EXPECT_EQ(tree.GetOr<std::string>("name", "fallback"), "RTRLab");
}

TEST(PropertyTreeTests, GetOrReturnsFallbackWhenTypeDoesNotMatchOrKeyIsMissing)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["count"] = PropertyTree(7);
    tree["scale"] = PropertyTree(1.5f);

    EXPECT_EQ(tree.GetOr<std::string>("count", "fallback"), "fallback");
    EXPECT_EQ(tree.GetOr<int>("scale", 42), 42);
    EXPECT_FALSE(tree.GetOr<bool>("missing", false));
}

TEST(PropertyTreeTests, GetOrFloatFromFloatValue)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["scale"] = PropertyTree(3.14);

    EXPECT_FLOAT_EQ(tree.GetOr<float>("scale", 0.0f), 3.14f);
}

TEST(PropertyTreeTests, GetOrAcceptsIntegersForFloatingPointRequests)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["count"] = PropertyTree(10);

    EXPECT_DOUBLE_EQ(tree.GetOr<double>("count", 0.0), 10.0);
}

TEST(PropertyTreeTests, GetOrReturnsFallbackForNonObjectTrees)
{
    PropertyTree arrayTree(PropertyTree::Array{PropertyTree(1), PropertyTree(2)});

    EXPECT_EQ(arrayTree.GetOr<int>("count", 99), 99);
}

TEST(PropertyTreeTests, GetOrSupportsInt64Values)
{
    const int64_t value = int64_t{1} << 40;
    PropertyTree tree = PropertyTree::Object{};
    tree["big"] = PropertyTree(value);

    EXPECT_EQ(tree.GetOr<int64_t>("big", 0), value);
}

TEST(PropertyTreeTests, ObjectIterationUsesStableSortedKeyOrder)
{
    PropertyTree tree = PropertyTree::Object{};
    tree["z"] = PropertyTree(1);
    tree["a"] = PropertyTree(2);
    tree["m"] = PropertyTree(3);

    std::vector<std::string> keys;
    for (const auto &[key, value] : tree.AsObject())
    {
        (void)value;
        keys.push_back(key);
    }

    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], "a");
    EXPECT_EQ(keys[1], "m");
    EXPECT_EQ(keys[2], "z");
}
