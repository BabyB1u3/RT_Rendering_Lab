#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/FileSystem.h"

namespace
{
    constexpr const char *kExistingConfig = "input/ShadowMapping.json";
    constexpr const char *kExistingAsset = "configs/input/ShadowMapping.json";
    constexpr const char *kTempConfig = "test-contract/AutoCopyConfig.json";

    void RemovePathIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    void RemoveDirectoryIfEmpty(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}

TEST(FileSystemContractTests, InitResolvesProjectRootAndCommonDirectories)
{
    FileSystem::Init();

    const auto &root = FileSystem::GetRootPath();
    const auto &savedDir = FileSystem::GetSavedDir();

    EXPECT_TRUE(std::filesystem::exists(root / "assets"));
    EXPECT_TRUE(std::filesystem::exists(savedDir));
    EXPECT_TRUE(std::filesystem::exists(savedDir / "configs"));
    EXPECT_TRUE(std::filesystem::exists(FileSystem::GetAssetPath(kExistingAsset)));
}

TEST(FileSystemContractTests, ResolveConfigPathPrefersSavedConfigWhenPresent)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveConfigPath(kExistingConfig);

    EXPECT_EQ(resolved, FileSystem::GetSavedConfigPath(kExistingConfig));
    EXPECT_TRUE(std::filesystem::exists(resolved));
}

TEST(FileSystemContractTests, ResolveConfigPathCopiesDefaultConfigIntoSavedDirectory)
{
    FileSystem::Init();

    const auto assetPath = FileSystem::GetAssetPath("configs") / kTempConfig;
    const auto savedPath = FileSystem::GetSavedConfigPath(kTempConfig);
    const std::string expectedContents = "{\n  \"source\": \"contract-test\"\n}\n";

    RemovePathIfExists(savedPath);
    RemovePathIfExists(assetPath);

    std::filesystem::create_directories(assetPath.parent_path());
    {
        std::ofstream out(assetPath, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out << expectedContents;
    }

    const auto resolved = FileSystem::ResolveConfigPath(kTempConfig);

    EXPECT_EQ(resolved, savedPath);
    ASSERT_TRUE(std::filesystem::exists(savedPath));
    EXPECT_EQ(FileSystem::ReadTextFile(savedPath), expectedContents);

    RemovePathIfExists(savedPath);
    RemovePathIfExists(assetPath);
    RemoveDirectoryIfEmpty(savedPath.parent_path());
    RemoveDirectoryIfEmpty(assetPath.parent_path());
}

TEST(FileSystemContractTests, ResolveConfigPathReturnsEmptyWhenConfigIsMissing)
{
    FileSystem::Init();

    const auto assetPath = FileSystem::GetAssetPath("configs") / "test-contract/MissingConfig.json";
    const auto savedPath = FileSystem::GetSavedConfigPath("test-contract/MissingConfig.json");

    RemovePathIfExists(savedPath);
    RemovePathIfExists(assetPath);

    const auto resolved = FileSystem::ResolveConfigPath("test-contract/MissingConfig.json");

    EXPECT_TRUE(resolved.empty());
}
