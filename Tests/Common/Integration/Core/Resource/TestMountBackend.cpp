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

    void TearDown() override { test_support::RemoveCurrentTestArtifacts("mount-backend"); }

    std::filesystem::path TestRoot() const { return m_TestRoot; }

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
    EXPECT_EQ(savedMount->m_Domain, Resource::PathDomain::Saved);
    EXPECT_EQ(savedMount->m_RootPath, savedDir);

    const auto cacheMount = Resource::ResolveWritableMount(Resource::PathDomain::Cache, savedDir, cacheDir);
    ASSERT_TRUE(cacheMount.has_value());
    EXPECT_EQ(cacheMount->m_Domain, Resource::PathDomain::Cache);
    EXPECT_EQ(cacheMount->m_RootPath, cacheDir);

    EXPECT_FALSE(Resource::ResolveWritableMount(Resource::PathDomain::Project, savedDir, cacheDir).has_value());
}

TEST_F(MountBackendTests, ResolveReadableMountArtifactReturnsPakBackedDescriptor)
{
    const auto sourceRoot = TestRoot() / "pak-source";
    const auto packagedRoot = TestRoot() / "out";
    const auto pakPath = test_support::GamePackagedArchivePath(packagedRoot);

    test_support::WriteTextFileOrFail(test_support::MountCatalogPath(sourceRoot),
                                      "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteMountFileOrFail(sourceRoot, "Materials/Checker.json", "{\n  \"name\": \"pak\"\n}\n");

    std::string errorMessage;
    ASSERT_TRUE(Resource::BuildPakArchive(sourceRoot, pakPath, &errorMessage)) << errorMessage;

    const Resource::ReadableMount mount{
        .m_CacheKey = "Packaged:Project",
        .m_SourceKey = "Project",
        .m_MountPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        .m_Priority = Resource::MountPriority::Packaged,
        .m_Backend = Resource::MountBackendKind::PakArchive,
        .m_MountRoot = pakPath,
    };
    const Resource::ArtifactRecord artifact{
        .m_RelativePath = "Materials/Checker.json",
        .m_Format = "json",
    };

    const auto resolved = Resource::ResolveReadableMountArtifact(mount, artifact, &errorMessage);
    ASSERT_TRUE(resolved.has_value()) << errorMessage;
    EXPECT_EQ(resolved->m_Backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(resolved->m_MountRoot, pakPath);
    EXPECT_EQ(resolved->m_RelativePath, std::filesystem::path("Materials/Checker.json"));

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
        .m_CacheKey = "Project",
        .m_SourceKey = "Project",
        .m_MountPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}},
        .m_Priority = Resource::MountPriority::Source,
        .m_Backend = Resource::MountBackendKind::Directory,
        .m_MountRoot = mountRoot,
    };
    const Resource::ArtifactRecord artifact{
        .m_RelativePath = "Docs/Readme.txt",
        .m_Format = "document",
    };

    std::string errorMessage;
    const auto resolved = Resource::ResolveReadableMountArtifact(mount, artifact, &errorMessage);
    ASSERT_TRUE(resolved.has_value()) << errorMessage;
    EXPECT_EQ(resolved->m_Backend, Resource::MountBackendKind::Directory);
    EXPECT_EQ(resolved->m_MountRoot, mountRoot);
    EXPECT_EQ(resolved->m_RelativePath, std::filesystem::path("Docs/Readme.txt"));

    const auto text = Resource::ReadReadableArtifactText(*resolved, &errorMessage);
    ASSERT_TRUE(text.has_value()) << errorMessage;
    EXPECT_EQ(*text, "hello");

    const auto stream = Resource::OpenReadableArtifactStream(*resolved, &errorMessage);
    ASSERT_NE(stream, nullptr) << errorMessage;
    const std::string streamedContents((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(streamedContents, "hello");
}
