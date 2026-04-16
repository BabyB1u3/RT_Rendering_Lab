#include <gtest/gtest.h>

#include "Core/Resource/FileSystem.h"

TEST(FileSystemVirtualPathTests, ParseProjectPathNormalizesRepeatedSlashes)
{
    const auto parsed = FileSystem::ParseVirtualPath("/Project//Config///input/DebugCameraControl.json");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->m_Domain, FileSystem::PathDomain::Project);
    EXPECT_FALSE(parsed->m_MountName.has_value());
    EXPECT_EQ(parsed->m_RelativePath, "Config/input/DebugCameraControl.json");
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

TEST(FileSystemVirtualPathTests, IsVirtualPathAcceptsReadableAndWritableDomains)
{
    EXPECT_TRUE(FileSystem::IsVirtualPath("/Project/Config/input/DebugCameraControl.json"));
    EXPECT_TRUE(FileSystem::IsVirtualPath("/Engine/Defaults/Materials/ErrorMaterial"));
    EXPECT_TRUE(FileSystem::IsVirtualPath("/DLC/Expansion1/Weapons/LaserRifle"));
    EXPECT_TRUE(FileSystem::IsVirtualPath("/Mod/CoolMod/Weapons/Hammer"));
    EXPECT_TRUE(FileSystem::IsVirtualPath("/Saved/Config/imgui.ini"));
    EXPECT_TRUE(FileSystem::IsVirtualPath("/Cache/Shaders/bootstrap.bin"));
}

TEST(FileSystemVirtualPathTests, ParseRecognizesSavedAndCacheDomains)
{
    const auto saved = FileSystem::ParseVirtualPath("/Saved/Logs/RTRLab.log");
    const auto cache = FileSystem::ParseVirtualPath("/Cache/Shaders/bootstrap.bin");

    ASSERT_TRUE(saved.has_value());
    EXPECT_EQ(saved->m_Domain, FileSystem::PathDomain::Saved);
    EXPECT_EQ(saved->m_RelativePath, "Logs/RTRLab.log");

    ASSERT_TRUE(cache.has_value());
    EXPECT_EQ(cache->m_Domain, FileSystem::PathDomain::Cache);
    EXPECT_EQ(cache->m_RelativePath, "Shaders/bootstrap.bin");
}

TEST(FileSystemVirtualPathTests, ParseRecognizesDlcAndModDomainsWithMountNames)
{
    const auto dlc = FileSystem::ParseVirtualPath("/DLC/Expansion1/Weapons/LaserRifle");
    const auto mod = FileSystem::ParseVirtualPath("/Mod/CoolMod/Weapons/Hammer");

    ASSERT_TRUE(dlc.has_value());
    EXPECT_EQ(dlc->m_Domain, FileSystem::PathDomain::DLC);
    ASSERT_TRUE(dlc->m_MountName.has_value());
    EXPECT_EQ(*dlc->m_MountName, "Expansion1");
    EXPECT_EQ(dlc->m_RelativePath, "Weapons/LaserRifle");

    ASSERT_TRUE(mod.has_value());
    EXPECT_EQ(mod->m_Domain, FileSystem::PathDomain::Mod);
    ASSERT_TRUE(mod->m_MountName.has_value());
    EXPECT_EQ(*mod->m_MountName, "CoolMod");
    EXPECT_EQ(mod->m_RelativePath, "Weapons/Hammer");
}

TEST(FileSystemVirtualPathTests, ParseRejectsDlcAndModPathsWithoutMountNames)
{
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/DLC").has_value());
    EXPECT_FALSE(FileSystem::ParseVirtualPath("/Mod").has_value());
}
