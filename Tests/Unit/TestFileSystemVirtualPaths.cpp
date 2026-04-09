#include <gtest/gtest.h>

#include "Core/Resource/FileSystem.h"

TEST(FileSystemVirtualPathTests, ParseProjectPathNormalizesRepeatedSlashes)
{
    const auto parsed = FileSystem::ParseVirtualPath("/Project//Config///input/DebugCameraControl.json");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->domain, FileSystem::PathDomain::Project);
    EXPECT_FALSE(parsed->mountName.has_value());
    EXPECT_EQ(parsed->relativePath, "Config/input/DebugCameraControl.json");
}

TEST(FileSystemVirtualPathTests, ParsePluginPathCapturesPluginMountName)
{
    const auto parsed = FileSystem::ParseVirtualPath("/Plugins/ExamplePlugin/Materials/Checker");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->domain, FileSystem::PathDomain::Plugin);
    ASSERT_TRUE(parsed->mountName.has_value());
    EXPECT_EQ(*parsed->mountName, "ExamplePlugin");
    EXPECT_EQ(parsed->relativePath, "Materials/Checker");
}

TEST(FileSystemVirtualPathTests, ParseRejectsPluginMountNamesOutsideIdentifierRules)
{
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Plugins/1BadPlugin/Materials/Checker").has_value());
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Plugins/Bad-Plugin/Materials/Checker").has_value());
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Plugins/Bad Plugin/Materials/Checker").has_value());
}

TEST(FileSystemVirtualPathTests, ParseRejectsMissingLeadingSlash)
{
    EXPECT_FALSE(FileSystem::ParseVirtualPath("Project/Textures/Wood").has_value());
}

TEST(FileSystemVirtualPathTests, ParseRejectsTraversalSegments)
{
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Project/Textures/../Wood").has_value());
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Project/./Textures/Wood").has_value());
}

TEST(FileSystemVirtualPathTests, ParseRejectsBackslashes)
{
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Project\\Textures\\Wood").has_value());
}

TEST(FileSystemVirtualPathTests, DocumentPathDetectionRequiresExplicitExtension)
{
    EXPECT_TRUE(FileSystem::IsDocumentPath("/Saved/Config/imgui.ini"));
    EXPECT_TRUE(FileSystem::IsDocumentPath("/Project/Config/input/DebugCameraControl.json"));
    EXPECT_FALSE(FileSystem::IsDocumentPath("/Project/Textures/Grassy_Square"));
}

TEST(FileSystemVirtualPathTests, CatalogBackedDetectionOnlyAppliesToReadDomainsWithoutExtensions)
{
    EXPECT_TRUE(FileSystem::IsCatalogBackedPath("/Project/Textures/Grassy_Square"));
    EXPECT_TRUE(FileSystem::IsCatalogBackedPath("/Plugins/Foo/Materials/Checker"));
    EXPECT_FALSE(FileSystem::IsCatalogBackedPath("/Project/Config/input/DebugCameraControl.json"));
    EXPECT_FALSE(FileSystem::IsCatalogBackedPath("/Saved/Logs/RTRLab.log"));
}
