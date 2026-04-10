#include <gtest/gtest.h>

#include <filesystem>
#include <iterator>
#include <string>

#include "Core/Resource/Catalog/SourceCatalog.h"
#include "ResourceTestSupport.h"

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
    test_support::WriteMountFileOrFail(TestRoot(), "textures/Grassy_Square.jpg", "jpg");
    test_support::WriteMountFileOrFail(TestRoot(), "Config/input/DebugCameraControl.json", "{}");
    test_support::WriteTextFileOrFail(test_support::MountCatalogPath(TestRoot()), "{}");

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

TEST_F(SourceCatalogTests, BuildSourceCatalogRejectsDuplicateLogicalPaths)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.jpg", "jpg");
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.png", "png");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    EXPECT_FALSE(Resource::BuildSourceCatalogEntries(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        entries,
        &errorMessage));
    EXPECT_NE(errorMessage.find("duplicate logical path"), std::string::npos);
}

TEST_F(SourceCatalogTests, IndexRepositorySourceCatalogsWritesProjectAndEngineCatalogs)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteProjectFileOrFail(repoRoot, "Textures/Grassy_Square.jpg", "jpg");
    test_support::WriteEngineFileOrFail(repoRoot, "Defaults/Materials/ErrorMaterial.json", "{\n}\n");

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(repoRoot, "Project", &errorMessage)) << errorMessage;

    const auto projectCatalogPath = test_support::ProjectSourceCatalogPath(repoRoot);
    const auto engineCatalogPath = test_support::EngineSourceCatalogPath(repoRoot);

    EXPECT_TRUE(std::filesystem::exists(projectCatalogPath));
    EXPECT_TRUE(std::filesystem::exists(engineCatalogPath));

    std::ifstream projectCatalog(projectCatalogPath);
    std::ifstream engineCatalog(engineCatalogPath);
    ASSERT_TRUE(projectCatalog.is_open());
    ASSERT_TRUE(engineCatalog.is_open());

    const std::string projectContents((std::istreambuf_iterator<char>(projectCatalog)), std::istreambuf_iterator<char>());
    const std::string engineContents((std::istreambuf_iterator<char>(engineCatalog)), std::istreambuf_iterator<char>());

    EXPECT_NE(projectContents.find("/Project/Textures/Grassy_Square"), std::string::npos);
    EXPECT_NE(engineContents.find("/Engine/Defaults/Materials/ErrorMaterial"), std::string::npos);
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
    EXPECT_FALSE(Resource::WriteSourceCatalogJson(test_support::MountCatalogPath(TestRoot()), entries, &errorMessage));
    EXPECT_NE(errorMessage.find("missing sourceRelativePath"), std::string::npos);
}
