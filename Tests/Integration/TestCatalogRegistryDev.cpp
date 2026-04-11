#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "ResourceTestSupport.h"

namespace
{
    class CatalogRegistryDevTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("catalog-registry-dev");
            test_support::ResetCurrentTestRoot("catalog-registry-dev");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("catalog-registry-dev");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(CatalogRegistryDevTests, CatalogRegistryCanReturnReadableArtifactDescriptorWithoutMaterializedPath)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteProjectFileOrFail(repoRoot, "Textures/Grassy_Square.jpg", "jpg");

    Resource::CatalogRegistry registry;
    const auto virtualPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Textures/Grassy_Square"};
    const auto resolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        virtualPath,
        "/Project/Textures/Grassy_Square",
        "Project");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->backend, Resource::MountBackendKind::Directory);
    EXPECT_EQ(resolved->mountRoot, test_support::ProjectContentRoot(repoRoot));
    EXPECT_EQ(resolved->relativePath, std::filesystem::path("Textures/Grassy_Square.jpg"));
}

TEST_F(CatalogRegistryDevTests, CatalogRegistryBuildsProjectConfigDocumentEntriesInMemoryDuringDevResolution)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteProjectFileOrFail(repoRoot, "Config/Graphics.json", "{\n  \"vsync\": true\n}\n");

    Resource::CatalogRegistry registry;
    const auto virtualPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Config/Graphics.json"};
    const auto resolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        virtualPath,
        "/Project/Config/Graphics.json",
        "Project");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->backend, Resource::MountBackendKind::Directory);
    EXPECT_EQ(resolved->mountRoot, test_support::ProjectContentRoot(repoRoot));
    EXPECT_EQ(resolved->relativePath, std::filesystem::path("Config/Graphics.json"));
}

TEST_F(CatalogRegistryDevTests, CatalogRegistryBuildsEngineSourceCatalogInMemoryDuringDevResolution)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteEngineFileOrFail(repoRoot, "Defaults/Materials/ErrorMaterial.json", "{\n}\n");

    Resource::CatalogRegistry registry;
    const auto virtualPath = Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, "Defaults/Materials/ErrorMaterial"};
    const auto resolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        virtualPath,
        "/Engine/Defaults/Materials/ErrorMaterial",
        "Project");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->backend, Resource::MountBackendKind::Directory);
    EXPECT_EQ(resolved->mountRoot, test_support::EngineRoot(repoRoot));
    EXPECT_EQ(resolved->relativePath, std::filesystem::path("Defaults/Materials/ErrorMaterial.json"));
}
