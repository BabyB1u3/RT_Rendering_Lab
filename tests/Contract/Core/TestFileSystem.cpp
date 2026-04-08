#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/Resource/FileSystem.h"

namespace
{
    constexpr const char *kExistingConfig = "input/DebugCameraControl.json";
    constexpr const char *kExistingAsset = "Config/input/DebugCameraControl.json";
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

    void RemoveDirectoryTreeIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    void WriteTextFileOrFail(const std::filesystem::path &path, std::string_view contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out << contents;
        ASSERT_TRUE(out.good());
    }
}

TEST(FileSystemContractTests, InitResolvesProjectRootAndCommonDirectories)
{
    FileSystem::Init();

    const auto &root = FileSystem::GetRootPath();
    const auto &savedDir = FileSystem::GetSavedDir();
    const auto &cacheDir = FileSystem::GetCacheDir();

    EXPECT_TRUE(std::filesystem::exists(root / "Content"));
    EXPECT_TRUE(std::filesystem::exists(savedDir));
    EXPECT_TRUE(std::filesystem::exists(cacheDir));
    EXPECT_TRUE(std::filesystem::exists(savedDir / "Config"));
    EXPECT_TRUE(std::filesystem::exists(FileSystem::GetAssetPath(kExistingAsset)));
}

TEST(FileSystemContractTests, ResolveReadPathMapsProjectConfigLogicalPathToContentConfigDirectory)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveReadPath("/Project/Config/input/DebugCameraControl.json");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetAssetPath("Config/input/DebugCameraControl.json"));
    EXPECT_TRUE(std::filesystem::exists(*resolved));
}

TEST(FileSystemContractTests, ResolveReadPathUsesCatalogForExtensionlessProjectAsset)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetAssetPath("textures/Grassy_Square.jpg"));
    EXPECT_TRUE(std::filesystem::exists(*resolved));
}

TEST(FileSystemContractTests, ResolveReadPathUsesCatalogForExtensionlessPluginAsset)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "CatalogTestPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"checker\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/CatalogTestPlugin/Materials/Checker\",\n"
        "      \"sourceRelativePath\": \"Materials/Checker.json\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Materials/Checker.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    const auto resolved = FileSystem::ResolveReadPath("/Plugins/CatalogTestPlugin/Materials/Checker");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, artifactPath);
    EXPECT_TRUE(std::filesystem::exists(*resolved));

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveWritePathMapsSavedConfigLogicalPathToSavedConfigDirectory)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveWritePath("/Saved/Config/test-contract/Phase12Config.json");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetSavedConfigPath("test-contract/Phase12Config.json"));
    EXPECT_TRUE(std::filesystem::exists(resolved->parent_path()));

    RemoveDirectoryTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
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

    RemoveDirectoryTreeIfExists(FileSystem::GetCacheDir() / "Shaders" / "opengl");
    RemoveDirectoryIfEmpty(FileSystem::GetCacheDir() / "Shaders");
}

TEST(FileSystemContractTests, ResolveReadPathRejectsInvalidMountsAndTraversal)
{
    FileSystem::Init();

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Unknown/Textures/Grassy_Square").has_value());
    EXPECT_FALSE(FileSystem::ResolveReadPath("/Project/Textures/../Grassy_Square").has_value());
}

TEST(FileSystemContractTests, ResolveReadPathRejectsCatalogWithDuplicateLogicalPaths)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "DuplicateCatalogPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"checker\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/DuplicateCatalogPlugin/Materials/Checker\",\n"
        "      \"sourceRelativePath\": \"Materials/Checker.json\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Materials/Checker.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    },\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/DuplicateCatalogPlugin/Materials/Checker\",\n"
        "      \"sourceRelativePath\": \"Materials/Checker.json\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Materials/Checker.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/DuplicateCatalogPlugin/Materials/Checker").has_value());

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ExistsSupportsLogicalPaths)
{
    FileSystem::Init();

    EXPECT_TRUE(FileSystem::Exists("/Project/Config/input/DebugCameraControl.json"));
    EXPECT_TRUE(FileSystem::Exists("/Project/Textures/Grassy_Square"));
    EXPECT_FALSE(FileSystem::Exists("/Project/Config/input/DefinitelyMissing.json"));
}

TEST(FileSystemContractTests, ReadTextSupportsLogicalProjectConfigPaths)
{
    FileSystem::Init();

    const auto contents = FileSystem::ReadText("/Project/Config/input/DebugCameraControl.json");

    ASSERT_TRUE(contents.has_value());
    EXPECT_FALSE(contents->empty());
}

TEST(FileSystemContractTests, ReadBinarySupportsCatalogBackedProjectAssetPaths)
{
    FileSystem::Init();

    const auto contents = FileSystem::ReadBinary("/Project/Textures/Grassy_Square");

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
    RemoveDirectoryTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
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
    RemoveDirectoryTreeIfExists(FileSystem::GetCacheDir() / "Shaders" / "test-contract");
    RemoveDirectoryIfEmpty(FileSystem::GetCacheDir() / "Shaders");
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

    const auto savedPath = FileSystem::GetSavedConfigPath(kExistingConfig);
    RemovePathIfExists(savedPath);
    ASSERT_TRUE(FileSystem::WriteText("/Saved/Config/input/DebugCameraControl.json", "{\n  \"source\": \"saved\"\n}\n"));

    const auto resolved = FileSystem::ResolveConfigPath(kExistingConfig);

    EXPECT_EQ(resolved, savedPath);
    EXPECT_TRUE(std::filesystem::exists(resolved));

    RemovePathIfExists(savedPath);
    RemoveDirectoryIfEmpty(savedPath.parent_path());
    RemoveDirectoryIfEmpty(savedPath.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveConfigPathCopiesDefaultConfigIntoSavedDirectory)
{
    FileSystem::Init();

    const auto assetPath = FileSystem::GetAssetPath("Config") / kTempConfig;
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
    RemoveDirectoryTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
    RemoveDirectoryIfEmpty(assetPath.parent_path());
}

TEST(FileSystemContractTests, ResolveConfigPathReturnsEmptyWhenConfigIsMissing)
{
    FileSystem::Init();

    const auto assetPath = FileSystem::GetAssetPath("Config") / "test-contract/MissingConfig.json";
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
