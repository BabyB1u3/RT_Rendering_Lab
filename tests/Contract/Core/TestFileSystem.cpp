#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/FileSystem.h"

namespace
{
    constexpr const char *kExistingConfig = "input/DebugCameraControl.json";
    constexpr const char *kExistingAsset = "configs/input/DebugCameraControl.json";
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
    const auto contents = FileSystem::ReadTextFile(savedPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_EQ(*contents, expectedContents);

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

// --- Error path tests ---

TEST(FileSystemContractTests, ReadTextFileReturnsNulloptOnMissingFile)
{
    FileSystem::Init();

    const auto contents = FileSystem::ReadTextFile("definitely/missing/path.txt");
    EXPECT_FALSE(contents.has_value());
}

TEST(FileSystemContractTests, ReadBinaryFileReturnsNulloptOnMissingFile)
{
    FileSystem::Init();

    const auto data = FileSystem::ReadBinaryFile("definitely/missing/binary.bin");
    EXPECT_FALSE(data.has_value());
}
