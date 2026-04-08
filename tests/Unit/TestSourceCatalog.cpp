#include <gtest/gtest.h>

#include <filesystem>
#include <iterator>
#include <string>

#include "Core/Resource/Catalog/SourceCatalog.h"
#include "TestPaths.h"

namespace
{
    class SourceCatalogTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("source-catalog");
            test_support::ResetCurrentTestRoot("source-catalog");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("source-catalog");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }
    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(SourceCatalogTests, BuildProjectSourceCatalogSkipsConfigAndCatalogArtifacts)
{
    test_support::WriteTextFileOrFail(TestRoot() / "textures" / "Grassy_Square.jpg", "jpg");
    test_support::WriteTextFileOrFail(TestRoot() / "Config" / "input" / "DebugCameraControl.json", "{}");
    test_support::WriteTextFileOrFail(TestRoot() / ".rtr" / "catalog.json", "{}");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogEntries(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        entries,
        &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].logicalPath, "/Project/Textures/Grassy_Square");
    ASSERT_TRUE(entries[0].sourceRelativePath.has_value());
    EXPECT_EQ(*entries[0].sourceRelativePath, "textures/Grassy_Square.jpg");
    ASSERT_EQ(entries[0].artifacts.size(), 1u);
    EXPECT_EQ(entries[0].artifacts[0].relativePath, "textures/Grassy_Square.jpg");
    EXPECT_EQ(entries[0].artifacts[0].format, "jpg");
    EXPECT_EQ(entries[0].artifacts[0].profileTag, "dev");
}

TEST_F(SourceCatalogTests, BuildPluginSourceCatalogUsesPluginNamespace)
{
    test_support::WriteTextFileOrFail(TestRoot() / "Materials" / "Checker.json", "{\n}\n");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogEntries(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Plugin, "ExamplePlugin", {}},
        entries,
        &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].logicalPath, "/Plugins/ExamplePlugin/Materials/Checker");
}

TEST_F(SourceCatalogTests, BuildPluginSourceCatalogRejectsInvalidMountName)
{
    test_support::WriteTextFileOrFail(TestRoot() / "Materials" / "Checker.json", "{\n}\n");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    EXPECT_FALSE(Resource::BuildSourceCatalogEntries(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Plugin, "Bad-Plugin", {}},
        entries,
        &errorMessage));
    EXPECT_NE(errorMessage.find("failed to derive logical path"), std::string::npos);
}

TEST_F(SourceCatalogTests, BuildSourceCatalogRejectsDuplicateLogicalPaths)
{
    test_support::WriteTextFileOrFail(TestRoot() / "Textures" / "Grassy_Square.jpg", "jpg");
    test_support::WriteTextFileOrFail(TestRoot() / "Textures" / "Grassy_Square.png", "png");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    EXPECT_FALSE(Resource::BuildSourceCatalogEntries(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        entries,
        &errorMessage));
    EXPECT_NE(errorMessage.find("duplicate logical path"), std::string::npos);
}

TEST_F(SourceCatalogTests, IndexRepositorySourceCatalogsWritesProjectEngineAndPluginCatalogs)
{
    const auto repoRoot = TestRoot() / "Repo";
    test_support::WriteTextFileOrFail(repoRoot / "Content" / "Textures" / "Grassy_Square.jpg", "jpg");
    test_support::WriteTextFileOrFail(repoRoot / "EngineContent" / "Defaults" / "Materials" / "ErrorMaterial.json", "{\n}\n");
    test_support::WriteTextFileOrFail(repoRoot / "Plugins" / "ExamplePlugin" / "Content" / "Materials" / "Checker.json", "{\n}\n");

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(repoRoot, "Content", &errorMessage)) << errorMessage;

    const auto projectCatalogPath = repoRoot / "Content" / ".rtr" / "catalog.json";
    const auto engineCatalogPath = repoRoot / "EngineContent" / ".rtr" / "catalog.json";
    const auto pluginCatalogPath = repoRoot / "Plugins" / "ExamplePlugin" / "Content" / ".rtr" / "catalog.json";

    EXPECT_TRUE(std::filesystem::exists(projectCatalogPath));
    EXPECT_TRUE(std::filesystem::exists(engineCatalogPath));
    EXPECT_TRUE(std::filesystem::exists(pluginCatalogPath));

    std::ifstream projectCatalog(projectCatalogPath);
    std::ifstream engineCatalog(engineCatalogPath);
    std::ifstream pluginCatalog(pluginCatalogPath);
    ASSERT_TRUE(projectCatalog.is_open());
    ASSERT_TRUE(engineCatalog.is_open());
    ASSERT_TRUE(pluginCatalog.is_open());

    const std::string projectContents((std::istreambuf_iterator<char>(projectCatalog)), std::istreambuf_iterator<char>());
    const std::string engineContents((std::istreambuf_iterator<char>(engineCatalog)), std::istreambuf_iterator<char>());
    const std::string pluginContents((std::istreambuf_iterator<char>(pluginCatalog)), std::istreambuf_iterator<char>());

    EXPECT_NE(projectContents.find("/Project/Textures/Grassy_Square"), std::string::npos);
    EXPECT_NE(engineContents.find("/Engine/Defaults/Materials/ErrorMaterial"), std::string::npos);
    EXPECT_NE(pluginContents.find("/Plugins/ExamplePlugin/Materials/Checker"), std::string::npos);
}

TEST_F(SourceCatalogTests, IndexRepositorySourceCatalogsSkipsPluginsWithInvalidMountNames)
{
    const auto repoRoot = TestRoot() / "Repo";
    test_support::WriteTextFileOrFail(repoRoot / "Plugins" / "ValidPlugin" / "Content" / "Materials" / "Checker.json", "{\n}\n");
    test_support::WriteTextFileOrFail(repoRoot / "Plugins" / "Bad-Plugin" / "Content" / "Materials" / "Ignored.json", "{\n}\n");

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(repoRoot, "Content", &errorMessage)) << errorMessage;

    const auto validCatalogPath = repoRoot / "Plugins" / "ValidPlugin" / "Content" / ".rtr" / "catalog.json";
    const auto invalidCatalogPath = repoRoot / "Plugins" / "Bad-Plugin" / "Content" / ".rtr" / "catalog.json";

    EXPECT_TRUE(std::filesystem::exists(validCatalogPath));
    EXPECT_FALSE(std::filesystem::exists(invalidCatalogPath));
}

TEST_F(SourceCatalogTests, WriteSourceCatalogJsonRejectsEntriesWithoutSourceRelativePath)
{
    std::vector<Resource::ResourceCatalogEntry> entries{
        Resource::ResourceCatalogEntry{
            .logicalPath = "/Project/Textures/Grassy_Square",
            .artifacts =
                {
                    Resource::ArtifactRecord{
                        .relativePath = "textures/Grassy_Square.jpg",
                        .format = "jpg",
                        .profileTag = "dev",
                    },
                },
        },
    };

    std::string errorMessage;
    EXPECT_FALSE(Resource::WriteSourceCatalogJson(TestRoot() / ".rtr" / "catalog.json", entries, &errorMessage));
    EXPECT_NE(errorMessage.find("missing sourceRelativePath"), std::string::npos);
}
