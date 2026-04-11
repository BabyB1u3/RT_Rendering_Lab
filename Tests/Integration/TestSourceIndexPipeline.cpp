#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "Core/Resource/Catalog/SourceCatalog.h"
#include "ResourceTestSupport.h"

namespace
{
    class SourceIndexPipelineTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("source-index-pipeline");
            test_support::ResetCurrentTestRoot("source-index-pipeline");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("source-index-pipeline");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(SourceIndexPipelineTests, IndexRepositorySourceCatalogsWritesProjectAndEngineCatalogs)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteProjectFileOrFail(repoRoot, "Textures/Grassy_Square.jpg", "jpg");
    test_support::WriteProjectFileOrFail(repoRoot, "Config/Graphics.json", "{\n}\n");
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
    EXPECT_NE(projectContents.find("/Project/Config/Graphics.json"), std::string::npos);
    EXPECT_NE(projectContents.find("\"format\": \"document\""), std::string::npos);
    EXPECT_NE(engineContents.find("/Engine/Defaults/Materials/ErrorMaterial"), std::string::npos);
}
