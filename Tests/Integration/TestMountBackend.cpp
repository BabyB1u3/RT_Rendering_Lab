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

TEST_F(MountBackendTests, ResolveReadableMountArtifactReturnsPakBackedDescriptor)
{
    const auto sourceRoot = TestRoot() / "pak-source";
    const auto packagedRoot = TestRoot() / "out";
    const auto pakPath = test_support::GamePackagedArchivePath(packagedRoot);

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
    };
    const Resource::ArtifactRecord artifact{
        .relativePath = "Materials/Checker.json",
        .format = "json",
    };

    const auto resolved = Resource::ResolveReadableMountArtifact(mount, artifact, &errorMessage);
    ASSERT_TRUE(resolved.has_value()) << errorMessage;
    EXPECT_EQ(resolved->backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(resolved->mountRoot, pakPath);
    EXPECT_EQ(resolved->relativePath, std::filesystem::path("Materials/Checker.json"));

    const auto bytes = Resource::ReadReadableArtifactBinary(*resolved, &errorMessage);
    ASSERT_TRUE(bytes.has_value()) << errorMessage;
    const std::string contents(bytes->begin(), bytes->end());
    EXPECT_NE(contents.find("\"pak\""), std::string::npos);

    const auto stream = Resource::OpenReadableArtifactStream(*resolved, &errorMessage);
    ASSERT_NE(stream, nullptr) << errorMessage;
    const std::string streamedContents((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(streamedContents, contents);
}

TEST_F(MountBackendTests, ResolveReadableMountArtifactReturnsDirectoryBackedDescriptor)
{
    const auto mountRoot = TestRoot() / "directory-mount";
    test_support::WriteMountFileOrFail(mountRoot, "Docs/Readme.txt", "hello");

    const Resource::ReadableMount mount{
        .cacheKey = "Project",
        .sourceKey = "Project",
        .mountPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        .priority = Resource::MountPriority::Source,
        .backend = Resource::MountBackendKind::Directory,
        .mountRoot = mountRoot,
    };
    const Resource::ArtifactRecord artifact{
        .relativePath = "Docs/Readme.txt",
        .format = "document",
    };

    std::string errorMessage;
    const auto resolved = Resource::ResolveReadableMountArtifact(mount, artifact, &errorMessage);
    ASSERT_TRUE(resolved.has_value()) << errorMessage;
    EXPECT_EQ(resolved->backend, Resource::MountBackendKind::Directory);
    EXPECT_EQ(resolved->mountRoot, mountRoot);
    EXPECT_EQ(resolved->relativePath, std::filesystem::path("Docs/Readme.txt"));

    const auto text = Resource::ReadReadableArtifactText(*resolved, &errorMessage);
    ASSERT_TRUE(text.has_value()) << errorMessage;
    EXPECT_EQ(*text, "hello");

    const auto stream = Resource::OpenReadableArtifactStream(*resolved, &errorMessage);
    ASSERT_NE(stream, nullptr) << errorMessage;
    const std::string streamedContents((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(streamedContents, "hello");
}
