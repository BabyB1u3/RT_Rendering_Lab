#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
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

    void TearDown() override { test_support::RemoveCurrentTestArtifacts("source-catalog"); }

    std::filesystem::path TestRoot() const { return m_TestRoot; }

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
        TestRoot(), Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}}, entries, &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 2u);

    const auto findEntry = [&](std::string_view logicalPath) -> const Resource::ResourceCatalogEntry*
    {
        for (const auto& entry : entries)
        {
            if (entry.m_LogicalPath == logicalPath)
                return &entry;
        }

        return nullptr;
    };

    const auto* textureEntry = findEntry("/Project/Textures/Grassy_Square");
    ASSERT_NE(textureEntry, nullptr);
    ASSERT_TRUE(textureEntry->m_SourceRelativePath.has_value());
    EXPECT_EQ(*textureEntry->m_SourceRelativePath, "textures/Grassy_Square.jpg");
    ASSERT_EQ(textureEntry->m_Artifacts.size(), 1u);
    EXPECT_EQ(textureEntry->m_Artifacts[0].m_RelativePath, "textures/Grassy_Square.jpg");
    EXPECT_EQ(textureEntry->m_Artifacts[0].m_Format, "jpg");
    EXPECT_EQ(textureEntry->m_Artifacts[0].m_ProfileTag, "dev");

    const auto* configEntry = findEntry("/Project/Config/input/DebugCameraControl.json");
    ASSERT_NE(configEntry, nullptr);
    ASSERT_TRUE(configEntry->m_SourceRelativePath.has_value());
    EXPECT_EQ(*configEntry->m_SourceRelativePath, "Config/input/DebugCameraControl.json");
    ASSERT_EQ(configEntry->m_Artifacts.size(), 1u);
    EXPECT_EQ(configEntry->m_Artifacts[0].m_RelativePath, "Config/input/DebugCameraControl.json");
    EXPECT_EQ(configEntry->m_Artifacts[0].m_Format, "document");
    EXPECT_EQ(configEntry->m_Artifacts[0].m_ProfileTag, "dev");
}

TEST_F(SourceCatalogTests, BuildSourceCatalogEntriesCanBeIndexedByLogicalPath)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.jpg", "jpg");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogEntries(
        TestRoot(), Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}}, entries, &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 1u);
    const auto it =
        std::find_if(entries.begin(),
                     entries.end(),
                     [](const auto& entry) { return entry.m_LogicalPath == "/Project/Textures/Grassy_Square"; });
    ASSERT_NE(it, entries.end());
    ASSERT_TRUE(it->m_SourceRelativePath.has_value());
    EXPECT_EQ(*it->m_SourceRelativePath, "Textures/Grassy_Square.jpg");
    ASSERT_EQ(it->m_Artifacts.size(), 1u);
    EXPECT_EQ(it->m_Artifacts[0].m_RelativePath, "Textures/Grassy_Square.jpg");
    EXPECT_EQ(it->m_Artifacts[0].m_Format, "jpg");
    EXPECT_EQ(it->m_Artifacts[0].m_PlatformTag, "any");
    EXPECT_EQ(it->m_Artifacts[0].m_BackendTag, "any");
    EXPECT_EQ(it->m_Artifacts[0].m_ProfileTag, "dev");
}

TEST_F(SourceCatalogTests, BuildSourceCatalogRejectsDuplicateLogicalPaths)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.jpg", "jpg");
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.png", "png");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    EXPECT_FALSE(Resource::BuildSourceCatalogEntries(
        TestRoot(), Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}}, entries, &errorMessage));
    EXPECT_NE(errorMessage.find("duplicate logical path"), std::string::npos);
}

TEST_F(SourceCatalogTests, BuildSourceCatalogEntriesRejectsDuplicateLogicalPaths)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.jpg", "jpg");
    test_support::WriteMountFileOrFail(TestRoot(), "Textures/Grassy_Square.png", "png");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    EXPECT_FALSE(Resource::BuildSourceCatalogEntries(
        TestRoot(), Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}}, entries, &errorMessage));
    EXPECT_NE(errorMessage.find("duplicate logical path"), std::string::npos);
}

TEST_F(SourceCatalogTests, BuildEngineSourceCatalogEntriesUseEngineNamespace)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Defaults/Materials/ErrorMaterial.json", "{\n}\n");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogEntries(
        TestRoot(), Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}}, entries, &errorMessage))
        << errorMessage;

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].m_LogicalPath, "/Engine/Defaults/Materials/ErrorMaterial");
}

TEST_F(SourceCatalogTests, BuildSourceCatalogTreatsPlainTextSupportFilesAsDocuments)
{
    test_support::WriteMountFileOrFail(TestRoot(), "Docs/Readme.txt", "hello");

    std::vector<Resource::ResourceCatalogEntry> entries;
    std::string errorMessage;

    ASSERT_TRUE(Resource::BuildSourceCatalogEntries(
        TestRoot(), Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}}, entries, &errorMessage))
        << errorMessage;

    const auto it = std::find_if(entries.begin(),
                                 entries.end(),
                                 [](const auto& entry) { return entry.m_LogicalPath == "/Project/Docs/Readme.txt"; });
    ASSERT_NE(it, entries.end());
    ASSERT_TRUE(it->m_SourceRelativePath.has_value());
    EXPECT_EQ(*it->m_SourceRelativePath, "Docs/Readme.txt");
    ASSERT_EQ(it->m_Artifacts.size(), 1u);
    EXPECT_EQ(it->m_Artifacts[0].m_RelativePath, "Docs/Readme.txt");
    EXPECT_EQ(it->m_Artifacts[0].m_Format, "document");
}
