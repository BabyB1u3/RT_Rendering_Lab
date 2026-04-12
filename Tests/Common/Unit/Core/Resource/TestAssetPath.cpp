#include <gtest/gtest.h>

#include "Core/Resource/Catalog/AssetPath.h"
#include "Core/Serialization/BuiltinTraits.h"

using namespace Serialization;

TEST(AssetPathTests, TryCreateAcceptsContentPathsWithoutExtensions)
{
    EXPECT_TRUE(Resource::AssetPath::TryCreate("/Project/Textures/Grassy_Square").has_value());
    EXPECT_TRUE(Resource::AssetPath::TryCreate("/Engine/Defaults/Materials/ErrorMaterial").has_value());
    EXPECT_TRUE(Resource::AssetPath::TryCreate("/DLC/Expansion1/Weapons/LaserRifle").has_value());
    EXPECT_TRUE(Resource::AssetPath::TryCreate("/Mod/CoolMod/Weapons/Hammer").has_value());
}

TEST(AssetPathTests, TryCreateRejectsConfigDocumentPaths)
{
    EXPECT_FALSE(Resource::AssetPath::TryCreate("/Project/Config/Graphics.json").has_value());
    EXPECT_FALSE(Resource::AssetPath::TryCreate("/Engine/Config/Graphics.json").has_value());
    EXPECT_FALSE(Resource::AssetPath::TryCreate("/DLC/Expansion1/Config/Graphics.json").has_value());
    EXPECT_FALSE(Resource::AssetPath::TryCreate("/Mod/CoolMod/Config/Graphics.json").has_value());
}

TEST(AssetPathTests, DeserializeRejectsAbsoluteFilesystemPathAndLeavesOutputUnchanged)
{
    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    Resource::AssetPath output = *original;

    EXPECT_FALSE(Deserialize(PropertyTree("C:/Users/name/dev/RTRLab/Project/textures/Grassy_Square.jpg"), output));
    EXPECT_EQ(output, *original);
}

TEST(AssetPathTests, DeserializeRejectsSavedDomainAndLeavesOutputUnchanged)
{
    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    Resource::AssetPath output = *original;

    EXPECT_FALSE(Deserialize(PropertyTree("/Saved/Config/Input/DebugCameraControl.json"), output));
    EXPECT_EQ(output, *original);
}

TEST(AssetPathTests, DeserializeRejectsDocumentPathAndLeavesOutputUnchanged)
{
    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    Resource::AssetPath output = *original;

    EXPECT_FALSE(Deserialize(PropertyTree("/Project/Config/input/DebugCameraControl.json"), output));
    EXPECT_EQ(output, *original);
}
