#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "Core/Resource/FileSystem.h"

namespace
{
    constexpr const char *kExistingConfig = "input/DebugCameraControl.json";
    constexpr const char *kExistingAsset = "Config/input/DebugCameraControl.json";
    constexpr const char *kTempConfig = "test-contract/AutoCopyConfig.json";

    std::string_view CurrentPlatformTag()
    {
#if defined(_WIN32)
        return "windows";
#elif defined(__APPLE__)
        return "macos";
#elif defined(__linux__)
        return "linux";
#else
        return "unknown";
#endif
    }

    std::string_view CurrentBackendTag()
    {
#if defined(GLAB_BACKEND_METAL)
        return "metal";
#else
        return "opengl";
#endif
    }

    constexpr std::string_view CurrentProfileTag()
    {
        return "dev";
    }

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

    class ScopedEnvVar
    {
    public:
        ScopedEnvVar(const char *name, std::string_view value)
            : m_Name(name)
        {
            if (const char *existing = std::getenv(name))
            {
                m_PreviousValue = existing;
                m_HadPreviousValue = true;
            }

            Set(value);
        }

        ~ScopedEnvVar()
        {
            if (m_HadPreviousValue)
                Set(*m_PreviousValue);
            else
                Clear();
        }

    private:
        void Set(std::string_view value)
        {
#if defined(_WIN32)
            _putenv_s(m_Name.c_str(), std::string(value).c_str());
#else
            setenv(m_Name.c_str(), std::string(value).c_str(), 1);
#endif
        }

        void Clear()
        {
#if defined(_WIN32)
            _putenv_s(m_Name.c_str(), "");
#else
            unsetenv(m_Name.c_str());
#endif
        }

        std::string m_Name;
        std::optional<std::string> m_PreviousValue;
        bool m_HadPreviousValue = false;
    };
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

TEST(FileSystemContractTests, ResolveReadPathCanSwitchBetweenSourceAndCookedProjectArtifacts)
{
    FileSystem::Init();

    const auto sourceResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(sourceResolved.has_value());
    EXPECT_EQ(*sourceResolved, FileSystem::GetAssetPath("textures/Grassy_Square.jpg"));

    const auto cookedRoot = FileSystem::GetCacheDir() / "Cooked" / "Project";
    const auto cookedArtifactPath = cookedRoot / "Textures" / "Grassy_Square.ktx2";
    const auto cookedCatalogPath = cookedRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(cookedRoot);

    WriteTextFileOrFail(cookedArtifactPath, "cooked");
    WriteTextFileOrFail(
        cookedCatalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Project/Textures/Grassy_Square\",\n"
        "      \"sourceRelativePath\": \"textures/Grassy_Square.jpg\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Textures/Grassy_Square.ktx2\",\n"
        "          \"format\": \"ktx2\",\n"
        "          \"profileTag\": \"cooked\",\n"
        "          \"backendTag\": \"any\",\n"
        "          \"platformTag\": \"any\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    {
        ScopedEnvVar profileOverride("RTRLAB_RESOURCE_PROFILE", "cooked");
        FileSystem::RefreshCatalogs();

        const auto cookedResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
        ASSERT_TRUE(cookedResolved.has_value());
        EXPECT_EQ(*cookedResolved, cookedArtifactPath);
    }

    FileSystem::RefreshCatalogs();
    const auto revertedResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(revertedResolved.has_value());
    EXPECT_EQ(*revertedResolved, FileSystem::GetAssetPath("textures/Grassy_Square.jpg"));

    RemoveDirectoryTreeIfExists(FileSystem::GetCacheDir() / "Cooked");
}

TEST(FileSystemContractTests, ResolveReadPathUsesCatalogForExtensionlessEngineAsset)
{
    FileSystem::Init();

    const auto engineRoot = FileSystem::GetRootPath() / "EngineContent";
    const auto artifactPath = engineRoot / "Defaults" / "Materials" / "ErrorMaterial.json";
    const auto catalogPath = engineRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(engineRoot);

    WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"error-material\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Engine/Defaults/Materials/ErrorMaterial\",\n"
        "      \"sourceRelativePath\": \"Defaults/Materials/ErrorMaterial.json\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Defaults/Materials/ErrorMaterial.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    const auto resolved = FileSystem::ResolveReadPath("/Engine/Defaults/Materials/ErrorMaterial");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, artifactPath);
    EXPECT_TRUE(std::filesystem::exists(*resolved));

    RemoveDirectoryTreeIfExists(engineRoot);
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

TEST(FileSystemContractTests, ResolveReadPathPrefersMostSpecificArtifactForCurrentRuntime)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "RuntimeSpecificCatalogPlugin" / "Content";
    const auto fallbackPath = pluginRoot / "Materials" / "Fallback.json";
    const auto backendSpecificPath = pluginRoot / "Materials" / "BackendSpecific.json";
    const auto runtimeSpecificPath = pluginRoot / "Materials" / "RuntimeSpecific.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(fallbackPath, "{\n  \"name\": \"fallback\"\n}\n");
    WriteTextFileOrFail(backendSpecificPath, "{\n  \"name\": \"backend\"\n}\n");
    WriteTextFileOrFail(runtimeSpecificPath, "{\n  \"name\": \"runtime\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        std::string("{\n") +
            "  \"version\": 1,\n"
            "  \"entries\": [\n"
            "    {\n"
            "      \"logicalPath\": \"/Plugins/RuntimeSpecificCatalogPlugin/Materials/Picker\",\n"
            "      \"sourceRelativePath\": \"Materials/RuntimeSpecific.json\",\n"
            "      \"artifacts\": [\n"
            "        {\n"
            "          \"relativePath\": \"Materials/Fallback.json\",\n"
            "          \"format\": \"json\",\n"
            "          \"profileTag\": \"any\",\n"
            "          \"backendTag\": \"any\",\n"
            "          \"platformTag\": \"any\"\n"
            "        },\n"
            "        {\n"
            "          \"relativePath\": \"Materials/BackendSpecific.json\",\n"
            "          \"format\": \"json\",\n"
            "          \"profileTag\": \"any\",\n"
            "          \"backendTag\": \"" + std::string(CurrentBackendTag()) + "\",\n"
            "          \"platformTag\": \"any\"\n"
            "        },\n"
            "        {\n"
            "          \"relativePath\": \"Materials/RuntimeSpecific.json\",\n"
            "          \"format\": \"json\",\n"
            "          \"profileTag\": \"" + std::string(CurrentProfileTag()) + "\",\n"
            "          \"backendTag\": \"" + std::string(CurrentBackendTag()) + "\",\n"
            "          \"platformTag\": \"" + std::string(CurrentPlatformTag()) + "\"\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");

    const auto resolved = FileSystem::ResolveReadPath("/Plugins/RuntimeSpecificCatalogPlugin/Materials/Picker");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, runtimeSpecificPath);

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveReadPathFallsBackToAnyArtifactWhenSpecificTagsDoNotMatch)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "FallbackCatalogPlugin" / "Content";
    const auto fallbackPath = pluginRoot / "Materials" / "Fallback.json";
    const auto mismatchedPath = pluginRoot / "Materials" / "Mismatched.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(fallbackPath, "{\n  \"name\": \"fallback\"\n}\n");
    WriteTextFileOrFail(mismatchedPath, "{\n  \"name\": \"mismatch\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        std::string("{\n") +
            "  \"version\": 1,\n"
            "  \"entries\": [\n"
            "    {\n"
            "      \"logicalPath\": \"/Plugins/FallbackCatalogPlugin/Materials/Picker\",\n"
            "      \"sourceRelativePath\": \"Materials/Fallback.json\",\n"
            "      \"artifacts\": [\n"
            "        {\n"
            "          \"relativePath\": \"Materials/Mismatched.json\",\n"
            "          \"format\": \"json\",\n"
            "          \"profileTag\": \"shipping\",\n"
            "          \"backendTag\": \"vulkan\",\n"
            "          \"platformTag\": \"linux\"\n"
            "        },\n"
            "        {\n"
            "          \"relativePath\": \"Materials/Fallback.json\",\n"
            "          \"format\": \"json\",\n"
            "          \"profileTag\": \"any\",\n"
            "          \"backendTag\": \"any\",\n"
            "          \"platformTag\": \"any\"\n"
            "        }\n"
            "      ]\n"
            "    }\n"
            "  ]\n"
            "}\n");

    const auto resolved = FileSystem::ResolveReadPath("/Plugins/FallbackCatalogPlugin/Materials/Picker");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, fallbackPath);

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

TEST(FileSystemContractTests, ResolveReadPathProjectCatalogStillWorksWhenAnotherMountCatalogIsInvalid)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "BrokenCatalogPlugin" / "Content";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/BrokenCatalogPlugin/Materials/Broken\",\n"
        "      \"sourceRelativePath\": \"Materials/Broken.json\",\n"
        "      \"artifacts\": []\n"
        "    }\n"
        "  ]\n"
        "}\n");

    const auto resolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetAssetPath("textures/Grassy_Square.jpg"));

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, RefreshCatalogsDiscoversPluginCatalogAddedAfterInitialLookup)
{
    FileSystem::Init();

    const auto initialProjectResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(initialProjectResolved.has_value());

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "LateBoundCatalogPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"late-bound\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/LateBoundCatalogPlugin/Materials/Checker\",\n"
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

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/LateBoundCatalogPlugin/Materials/Checker").has_value());

    FileSystem::RefreshCatalogs();

    const auto refreshedResolved = FileSystem::ResolveReadPath("/Plugins/LateBoundCatalogPlugin/Materials/Checker");
    ASSERT_TRUE(refreshedResolved.has_value());
    EXPECT_EQ(*refreshedResolved, artifactPath);

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, RefreshCatalogsDropsPluginCatalogRemovedAfterInitialLookup)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "RemovedCatalogPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"removable\"\n}\n");
    WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/RemovedCatalogPlugin/Materials/Checker\",\n"
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

    const auto initialResolved = FileSystem::ResolveReadPath("/Plugins/RemovedCatalogPlugin/Materials/Checker");
    ASSERT_TRUE(initialResolved.has_value());
    EXPECT_EQ(*initialResolved, artifactPath);

    RemoveDirectoryTreeIfExists(pluginRoot.parent_path().parent_path());

    FileSystem::RefreshCatalogs();

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/RemovedCatalogPlugin/Materials/Checker").has_value());
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
