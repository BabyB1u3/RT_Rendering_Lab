#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Package/PakArchive.h"
#include "ResourceTestSupport.h"

namespace
{
    class MountBackendDevTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("mount-backend-dev");
            test_support::ResetCurrentTestRoot("mount-backend-dev");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("mount-backend-dev");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(MountBackendDevTests, DiscoverReadableMountBackendsFindsSourceDirectoryMounts)
{
    const auto repoRoot = TestRoot() / "repo";
    test_support::WriteProjectMarkerOrFail(repoRoot);

    test_support::EnsureDirectories(test_support::ProjectContentRoot(repoRoot));
    test_support::EnsureDirectories(test_support::EngineRoot(repoRoot));

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), test_support::CookedRoot(repoRoot), "Project", "dev");

    ASSERT_EQ(mounts.size(), 2u);
    EXPECT_EQ(mounts[0].sourceKey, "Project");
    EXPECT_EQ(mounts[0].priority, Resource::MountPriority::Source);
    EXPECT_EQ(mounts[0].backend, Resource::MountBackendKind::Directory);

    EXPECT_EQ(mounts[1].sourceKey, "Engine");
    EXPECT_EQ(mounts[1].priority, Resource::MountPriority::Source);
    EXPECT_EQ(mounts[1].backend, Resource::MountBackendKind::Directory);
}

TEST_F(MountBackendDevTests, DiscoverReadableMountBackendsUsesGameArchiveForPackagedProfile)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
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

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), repoRoot / "Saved" / "Cache", "Project", "packaged");

    ASSERT_EQ(mounts.size(), 2u);
    EXPECT_EQ(mounts[0].sourceKey, "Project");
    EXPECT_EQ(mounts[0].backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[0].mountRoot, test_support::GamePackagedArchivePath(packagedRoot));
    EXPECT_EQ(mounts[1].sourceKey, "Engine");
    EXPECT_EQ(mounts[1].backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[1].mountRoot, test_support::GamePackagedArchivePath(packagedRoot));
}
