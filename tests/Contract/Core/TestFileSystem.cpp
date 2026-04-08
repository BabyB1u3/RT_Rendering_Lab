#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/Resource/FileSystem.h"

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
    const auto &cacheDir = FileSystem::GetCacheDir();

    EXPECT_TRUE(std::filesystem::exists(root / "assets"));
    EXPECT_TRUE(std::filesystem::exists(savedDir));
    EXPECT_TRUE(std::filesystem::exists(cacheDir));
    EXPECT_TRUE(std::filesystem::exists(savedDir / "configs"));
    EXPECT_TRUE(std::filesystem::exists(FileSystem::GetAssetPath(kExistingAsset)));
}

TEST(FileSystemContractTests, ResolveReadPathMapsProjectConfigLogicalPathToLegacyConfigsDirectory)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveReadPath("/Project/Config/input/DebugCameraControl.json");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetAssetPath("configs/input/DebugCameraControl.json"));
    EXPECT_TRUE(std::filesystem::exists(*resolved));
}

TEST(FileSystemContractTests, ResolveWritePathMapsSavedConfigLogicalPathToLegacyConfigsDirectory)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveWritePath("/Saved/Config/test-contract/Phase12Config.json");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetSavedConfigPath("test-contract/Phase12Config.json"));
    EXPECT_TRUE(std::filesystem::exists(resolved->parent_path()));
}

TEST(FileSystemContractTests, ResolveWritePathRejectsReadOnlyDomains)
{
    FileSystem::Init();

    EXPECT_FALSE(FileSystem::ResolveWritePath("/Project/Config/input/DebugCameraControl.json").has_value());
    EXPECT_FALSE(FileSystem::ResolveWritePath("/Engine/Config/input/DebugCameraControl.json").has_value());
    EXPECT_FALSE(FileSystem::ResolveWritePath("/Plugins/Foo/Config/Test.json").has_value());
}

TEST(FileSystemContractTests, ResolveWritePathCreatesCacheDirectoryParents)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveWritePath("/Cache/Shaders/opengl/ForwardLit.cache");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetCacheDir() / "Shaders" / "opengl" / "ForwardLit.cache");
    EXPECT_TRUE(std::filesystem::exists(resolved->parent_path()));
}

TEST(FileSystemContractTests, ResolveReadPathRejectsInvalidMountsAndTraversal)
{
    FileSystem::Init();

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Unknown/Textures/Grassy_Square").has_value());
    EXPECT_FALSE(FileSystem::ResolveReadPath("/Project/Textures/../Grassy_Square").has_value());
}

TEST(FileSystemContractTests, ExistsSupportsLogicalPaths)
{
    FileSystem::Init();

    EXPECT_TRUE(FileSystem::Exists("/Project/Config/input/DebugCameraControl.json"));
    EXPECT_FALSE(FileSystem::Exists("/Project/Config/input/DefinitelyMissing.json"));
}

TEST(FileSystemContractTests, ReadTextSupportsLogicalProjectConfigPaths)
{
    FileSystem::Init();

    const auto contents = FileSystem::ReadText("/Project/Config/input/DebugCameraControl.json");

    ASSERT_TRUE(contents.has_value());
    EXPECT_FALSE(contents->empty());
}

TEST(FileSystemContractTests, WriteTextSupportsSavedLogicalPaths)
{
    FileSystem::Init();

    constexpr std::string_view kVirtualPath = "/Saved/Config/test-contract/LogicalWriteText.json";
    constexpr std::string_view kExpectedContents = "{\n  \"path\": \"logical\"\n}\n";
    const auto savedPath = FileSystem::GetSavedConfigPath("test-contract/LogicalWriteText.json");

    RemovePathIfExists(savedPath);

    ASSERT_TRUE(FileSystem::WriteText(kVirtualPath, kExpectedContents));
    const auto contents = FileSystem::ReadText(kVirtualPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_EQ(*contents, kExpectedContents);

    RemovePathIfExists(savedPath);
    RemoveDirectoryIfEmpty(savedPath.parent_path());
}

TEST(FileSystemContractTests, WriteBinarySupportsCacheLogicalPaths)
{
    FileSystem::Init();

    constexpr std::string_view kVirtualPath = "/Cache/Shaders/test-contract/ForwardLit.bin";
    const std::vector<uint8_t> expectedData{0x10, 0x20, 0x30, 0x40};
    const auto cachePath = FileSystem::GetCacheDir() / "Shaders" / "test-contract" / "ForwardLit.bin";

    RemovePathIfExists(cachePath);

    ASSERT_TRUE(FileSystem::WriteBinary(kVirtualPath, expectedData));
    const auto contents = FileSystem::ReadBinary(kVirtualPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_EQ(*contents, expectedData);

    RemovePathIfExists(cachePath);
    RemoveDirectoryIfEmpty(cachePath.parent_path());
}

TEST(FileSystemContractTests, WriteHelpersRejectReadOnlyDomains)
{
    FileSystem::Init();

    EXPECT_FALSE(FileSystem::WriteText("/Project/Config/input/Nope.json", "{}"));
    EXPECT_FALSE(FileSystem::WriteBinary("/Engine/Config/input/Nope.bin", std::vector<uint8_t>{1, 2, 3}));
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
