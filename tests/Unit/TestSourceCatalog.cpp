#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "Core/Resource/SourceCatalog.h"

namespace
{
    class SourceCatalogTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = std::filesystem::current_path() / "test-output" / "source-catalog" /
                         ::testing::UnitTest::GetInstance()->current_test_info()->name();

            std::error_code ec;
            std::filesystem::remove_all(m_TestRoot, ec);
            std::filesystem::create_directories(m_TestRoot);
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::current_path() / "test-output" / "source-catalog", ec);
            ec.clear();
            std::filesystem::remove(std::filesystem::current_path() / "test-output", ec);
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

        void WriteTextFile(const std::filesystem::path &path, std::string_view contents) const
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::binary);
            ASSERT_TRUE(out.is_open());
            out << contents;
            ASSERT_TRUE(out.good());
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(SourceCatalogTests, BuildProjectSourceCatalogSkipsConfigAndCatalogArtifacts)
{
    WriteTextFile(TestRoot() / "textures" / "Grassy_Square.jpg", "jpg");
    WriteTextFile(TestRoot() / "Config" / "input" / "DebugCameraControl.json", "{}");
    WriteTextFile(TestRoot() / ".rtr" / "catalog.json", "{}");

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
    EXPECT_EQ(entries[0].sourceRelativePath, "textures/Grassy_Square.jpg");
    ASSERT_EQ(entries[0].artifacts.size(), 1u);
    EXPECT_EQ(entries[0].artifacts[0].relativePath, "textures/Grassy_Square.jpg");
    EXPECT_EQ(entries[0].artifacts[0].format, "jpg");
    EXPECT_EQ(entries[0].artifacts[0].profileTag, "dev");
}

TEST_F(SourceCatalogTests, BuildPluginSourceCatalogUsesPluginNamespace)
{
    WriteTextFile(TestRoot() / "Materials" / "Checker.json", "{\n}\n");

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

TEST_F(SourceCatalogTests, BuildSourceCatalogRejectsDuplicateLogicalPaths)
{
    WriteTextFile(TestRoot() / "Textures" / "Grassy_Square.jpg", "jpg");
    WriteTextFile(TestRoot() / "Textures" / "Grassy_Square.png", "png");

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
    WriteTextFile(repoRoot / "Content" / "Textures" / "Grassy_Square.jpg", "jpg");
    WriteTextFile(repoRoot / "EngineContent" / "Defaults" / "Materials" / "ErrorMaterial.json", "{\n}\n");
    WriteTextFile(repoRoot / "Plugins" / "ExamplePlugin" / "Content" / "Materials" / "Checker.json", "{\n}\n");

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
