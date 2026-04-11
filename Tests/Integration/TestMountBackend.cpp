#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Package/PakArchive.h"
#include "ResourceTestSupport.h"

namespace
{
    class MountBackendTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("mount-backend");
            test_support::ResetCurrentTestRoot("mount-backend");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("mount-backend");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(MountBackendTests, ResolveWritableMountReturnsSavedAndCacheRoots)
{
    const auto savedDir = TestRoot() / "Saved";
    const auto cacheDir = TestRoot() / "Cache";

    const auto savedMount = Resource::ResolveWritableMount(Resource::PathDomain::Saved, savedDir, cacheDir);
    ASSERT_TRUE(savedMount.has_value());
    EXPECT_EQ(savedMount->domain, Resource::PathDomain::Saved);
    EXPECT_EQ(savedMount->rootPath, savedDir);

    const auto cacheMount = Resource::ResolveWritableMount(Resource::PathDomain::Cache, savedDir, cacheDir);
    ASSERT_TRUE(cacheMount.has_value());
    EXPECT_EQ(cacheMount->domain, Resource::PathDomain::Cache);
    EXPECT_EQ(cacheMount->rootPath, cacheDir);

    EXPECT_FALSE(Resource::ResolveWritableMount(Resource::PathDomain::Project, savedDir, cacheDir).has_value());
}

TEST_F(MountBackendTests, ResolveReadableMountArtifactMaterializesPakArchiveEntries)
{
    const auto sourceRoot = TestRoot() / "pak-source";
    const auto packagedRoot = TestRoot() / "out";
    const auto pakPath = test_support::ProjectPackagedArchivePath(packagedRoot);
    const auto extractedRoot = TestRoot() / "materialized";
    const auto materializedRoot = test_support::ProjectMaterializedRoot(extractedRoot);

    test_support::WriteTextFileOrFail(test_support::MountCatalogPath(sourceRoot), "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteMountFileOrFail(sourceRoot, "Materials/Checker.json", "{\n  \"name\": \"pak\"\n}\n");

    std::string errorMessage;
    ASSERT_TRUE(Resource::BuildPakArchive(sourceRoot, pakPath, &errorMessage)) << errorMessage;

    const Resource::ReadableMount mount{
        .cacheKey = "Packaged:Project",
        .sourceKey = "Project",
        .mountPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        .priority = Resource::MountPriority::Packaged,
        .backend = Resource::MountBackendKind::PakArchive,
        .mountRoot = pakPath,
        .materializedRoot = materializedRoot,
    };
    const Resource::ArtifactRecord artifact{
        .relativePath = "Materials/Checker.json",
        .format = "json",
    };

    const auto resolved = Resource::ResolveReadableMountArtifact(mount, artifact, &errorMessage);
    ASSERT_TRUE(resolved.has_value()) << errorMessage;
    EXPECT_EQ(*resolved, materializedRoot / "Materials" / "Checker.json");

    std::ifstream in(*resolved, std::ios::binary);
    ASSERT_TRUE(in.is_open());
    const std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("\"pak\""), std::string::npos);
}
