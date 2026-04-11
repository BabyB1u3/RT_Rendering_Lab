#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Package/PakArchive.h"
#include "RootDiscoveryTestSupport.h"
#include "ResourceTestSupport.h"

namespace
{
    using MountDiscoveryShippingTests = test_support::RootDiscoveryTestsBase;

    void BuildSharedGameArchiveFixture(const std::filesystem::path &repoRoot)
    {
        const auto cookedRoot = test_support::CookedRoot(repoRoot);
        const auto packagedRoot = repoRoot / "Saved" / "Cache" / "Packaged";

        test_support::WriteTextFileOrFail(
            test_support::ProjectCookedCatalogPath(cookedRoot),
            "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": \"/Project/Textures/Grassy_Square\",\n      \"artifacts\": [\n        {\n          \"relativePath\": \"Textures/Grassy_Square.rtrtex\",\n          \"format\": \"rtrtex\",\n          \"profileTag\": \"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    }\n  ]\n}\n");
        test_support::WriteProjectCookedFileOrFail(cookedRoot, "Textures/Grassy_Square.rtrtex", "project");

        test_support::WriteTextFileOrFail(
            test_support::EngineCookedCatalogPath(cookedRoot),
            "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": \"/Engine/Defaults/Materials/ErrorMaterial\",\n      \"artifacts\": [\n        {\n          \"relativePath\": \"Defaults/Materials/ErrorMaterial.json\",\n          \"format\": \"json\",\n          \"profileTag\": \"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    }\n  ]\n}\n");
        test_support::WriteEngineCookedFileOrFail(cookedRoot, "Defaults/Materials/ErrorMaterial.json", "engine");

        std::string errorMessage;
        ASSERT_TRUE(Resource::PackageCookedRepositoryCatalogs(cookedRoot, packagedRoot, &errorMessage)) << errorMessage;
    }
} // namespace

TEST_F(MountDiscoveryShippingTests, DiscoverReadableMountBackendsUsesGameArchiveForPackagedProfile)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    const auto packagedRoot = repoRoot / "Saved" / "Cache" / "Packaged";
    BuildSharedGameArchiveFixture(repoRoot);

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), repoRoot / "Saved" / "Cache", "Project", "packaged");

    ASSERT_EQ(mounts.size(), 4u);
    EXPECT_EQ(mounts[0].sourceKey, "Project");
    EXPECT_EQ(mounts[0].priority, Resource::MountPriority::Packaged);
    EXPECT_EQ(mounts[0].backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[0].mountRoot, test_support::GamePackagedArchivePath(packagedRoot));
    EXPECT_EQ(mounts[1].sourceKey, "Project");
    EXPECT_EQ(mounts[1].priority, Resource::MountPriority::Cooked);
    EXPECT_EQ(mounts[1].backend, Resource::MountBackendKind::Directory);
    EXPECT_EQ(mounts[2].sourceKey, "Engine");
    EXPECT_EQ(mounts[2].priority, Resource::MountPriority::Packaged);
    EXPECT_EQ(mounts[2].backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[2].mountRoot, test_support::GamePackagedArchivePath(packagedRoot));
    EXPECT_EQ(mounts[3].sourceKey, "Engine");
    EXPECT_EQ(mounts[3].priority, Resource::MountPriority::Cooked);
    EXPECT_EQ(mounts[3].backend, Resource::MountBackendKind::Directory);
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryResolvesProjectAndEngineEntriesFromSharedGameArchive)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    const auto packagedRoot = repoRoot / "Saved" / "Cache" / "Packaged";
    BuildSharedGameArchiveFixture(repoRoot);
    test_support::ScopedEnvVar packagedProfile("RTRLAB_RESOURCE_PROFILE", "packaged");

    Resource::CatalogRegistry registry;

    const auto projectVirtualPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Textures/Grassy_Square"};
    const auto projectResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        projectVirtualPath,
        "/Project/Textures/Grassy_Square",
        "Project");
    ASSERT_TRUE(projectResolved.has_value());
    EXPECT_EQ(projectResolved->backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(projectResolved->mountRoot, test_support::GamePackagedArchivePath(packagedRoot));
    EXPECT_EQ(projectResolved->relativePath, std::filesystem::path("Project/Textures/Grassy_Square.rtrtex"));
    std::string errorMessage;
    const auto projectBytes = Resource::ReadReadableArtifactBinary(*projectResolved, &errorMessage);
    ASSERT_TRUE(projectBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(projectBytes->begin(), projectBytes->end()), "project");

    const auto engineVirtualPath = Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, "Defaults/Materials/ErrorMaterial"};
    const auto engineResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        engineVirtualPath,
        "/Engine/Defaults/Materials/ErrorMaterial",
        "Project");
    ASSERT_TRUE(engineResolved.has_value());
    EXPECT_EQ(engineResolved->backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(engineResolved->mountRoot, test_support::GamePackagedArchivePath(packagedRoot));
    EXPECT_EQ(engineResolved->relativePath, std::filesystem::path("Engine/Defaults/Materials/ErrorMaterial.json"));
    const auto engineText = Resource::ReadReadableArtifactText(*engineResolved, &errorMessage);
    ASSERT_TRUE(engineText.has_value()) << errorMessage;
    EXPECT_EQ(*engineText, "engine");
}
