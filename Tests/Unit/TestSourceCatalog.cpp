#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <unordered_map>

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

TEST_F(SourceCatalogTests, BuildProjectSourceCatalogIncludesConfigDocumentsAndSkipsCatalogArtifacts)
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

    ASSERT_EQ(entries.size(), 2u);

    const auto findEntry = [&](std::string_view logicalPath) -> const Resource::ResourceCatalogEntry * {
        for (const auto &entry : entries)
        {
            if (entry.logicalPath == logicalPath)
                return &entry;
        }

        return nullptr;
    };

    const auto *textureEntry = findEntry("/Project/Textures/Grassy_Square");
    ASSERT_NE(textureEntry, nullptr);
    ASSERT_TRUE(textureEntry->sourceRelativePath.has_value());
    EXPECT_EQ(*textureEntry->sourceRelativePath, "textures/Grassy_Square.jpg");
    ASSERT_EQ(textureEntry->artifacts.size(), 1u);
    EXPECT_EQ(textureEntry->artifacts[0].relativePath, "textures/Grassy_Square.jpg");
    EXPECT_EQ(textureEntry->artifacts[0].format, "jpg");
    EXPECT_EQ(textureEntry->artifacts[0].profileTag, "dev");

    const auto *configEntry = findEntry("/Project/Config/input/DebugCameraControl.json");
    ASSERT_NE(configEntry, nullptr);
    ASSERT_TRUE(configEntry->sourceRelativePath.has_value());
    EXPECT_EQ(*configEntry->sourceRelativePath, "Config/input/DebugCameraControl.json");
    ASSERT_EQ(configEntry->artifacts.size(), 1u);
    EXPECT_EQ(configEntry->artifacts[0].relativePath, "Config/input/DebugCameraControl.json");
    EXPECT_EQ(configEntry->artifacts[0].format, "document");
    EXPECT_EQ(configEntry->artifacts[0].profileTag, "dev");
}

TEST_F(SourceCatalogTests, BuildSourceCatalogMapReturnsEntriesByLogicalPath)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.jpg", "jpg");

    std::unordered_map<std::string, Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogMap(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        entries,
        &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 1u);
    const auto it = entries.find("/Project/Textures/Grassy_Square");
    ASSERT_NE(it, entries.end());
    ASSERT_TRUE(it->second.sourceRelativePath.has_value());
    EXPECT_EQ(*it->second.sourceRelativePath, "Textures/Grassy_Square.jpg");
    ASSERT_EQ(it->second.artifacts.size(), 1u);
    EXPECT_EQ(it->second.artifacts[0].relativePath, "Textures/Grassy_Square.jpg");
    EXPECT_EQ(it->second.artifacts[0].format, "jpg");
    EXPECT_EQ(it->second.artifacts[0].platformTag, "any");
    EXPECT_EQ(it->second.artifacts[0].backendTag, "any");
    EXPECT_EQ(it->second.artifacts[0].profileTag, "dev");
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

TEST_F(SourceCatalogTests, BuildSourceCatalogMapRejectsDuplicateLogicalPaths)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.jpg", "jpg");
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.png", "png");

    std::unordered_map<std::string, Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    EXPECT_FALSE(Resource::BuildSourceCatalogMap(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        entries,
        &errorMessage));
    EXPECT_NE(errorMessage.find("duplicate logical path"), std::string::npos);
}

TEST_F(SourceCatalogTests, BuildEngineSourceCatalogMapUsesEngineNamespace)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Defaults/Materials/ErrorMaterial.json", "{\n}\n");

    std::unordered_map<std::string, Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogMap(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}},
        entries,
        &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries.contains("/Engine/Defaults/Materials/ErrorMaterial"));
}

TEST_F(SourceCatalogTests, BuildSourceCatalogTreatsPlainTextSupportFilesAsDocuments)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Docs/Readme.txt", "hello");

    std::unordered_map<std::string, Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogMap(
        TestRoot(),
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        entries,
        &errorMessage))
        << errorMessage;

    const auto it = entries.find("/Project/Docs/Readme.txt");
    ASSERT_NE(it, entries.end());
    ASSERT_TRUE(it->second.sourceRelativePath.has_value());
    EXPECT_EQ(*it->second.sourceRelativePath, "Docs/Readme.txt");
    ASSERT_EQ(it->second.artifacts.size(), 1u);
    EXPECT_EQ(it->second.artifacts[0].relativePath, "Docs/Readme.txt");
    EXPECT_EQ(it->second.artifacts[0].format, "document");
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
