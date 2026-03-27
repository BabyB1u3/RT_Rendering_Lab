#include <gtest/gtest.h>

#include "core/serialization/PropertyTree.h"

using Serialization::PropertyTree;

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
