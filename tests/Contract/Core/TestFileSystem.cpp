#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include "Core/Resource/CookedCatalog.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Resource/PakArchive.h"
#include "Core/Resource/SourceCatalog.h"
#include "TestPaths.h"

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

TEST(FileSystemContractTests, ResolveReadPathMapsEngineConfigLogicalPathToEngineContentConfigDirectory)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveReadPath("/Engine/Config/input/DefaultBindings.json");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetRootPath() / "EngineContent" / "Config" / "input" / "DefaultBindings.json");
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
    const auto cookedArtifactPath = cookedRoot / "Textures" / "Grassy_Square.rtrtex";
    const auto cookedCatalogPath = cookedRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(cookedRoot);

    test_support::WriteTextFileOrFail(cookedArtifactPath, "cooked");
    test_support::WriteTextFileOrFail(
        cookedCatalogPath,
        "{\n"
        "  \"version\": 2,\n"
        "  \"kind\": \"cooked\",\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Project/Textures/Grassy_Square\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Textures/Grassy_Square.rtrtex\",\n"
        "          \"format\": \"rtrtex\",\n"
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

    test_support::RemoveTreeIfExists(FileSystem::GetCacheDir() / "Cooked");
}

TEST(FileSystemContractTests, ResolveReadPathCanUseBuildCookedProjectArtifacts)
{
    FileSystem::Init();

    const auto buildCookedRoot = FileSystem::GetRootPath() / "build" / "Cooked" / "Project";
    const auto cookedArtifactPath = buildCookedRoot / "Textures" / "Grassy_Square.rtrtex";
    const auto cookedCatalogPath = buildCookedRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(FileSystem::GetCacheDir() / "Cooked");
    test_support::RemoveTreeIfExists(FileSystem::GetRootPath() / "build" / "Cooked");

    test_support::WriteTextFileOrFail(cookedArtifactPath, "build-cooked");
    test_support::WriteTextFileOrFail(
        cookedCatalogPath,
        "{\n"
        "  \"version\": 2,\n"
        "  \"kind\": \"cooked\",\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Project/Textures/Grassy_Square\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Textures/Grassy_Square.rtrtex\",\n"
        "          \"format\": \"rtrtex\",\n"
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

    test_support::RemoveTreeIfExists(FileSystem::GetRootPath() / "build" / "Cooked");
    FileSystem::RefreshCatalogs();
}

TEST(FileSystemContractTests, ResolveReadPathCanSwitchBetweenSourceAndCookedEngineAndPluginArtifacts)
{
    FileSystem::Init();

    const auto engineRoot = FileSystem::GetRootPath() / "EngineContent";
    const auto engineArtifactPath = engineRoot / "Defaults" / "Materials" / "Phase12ParityMaterial.json";
    const auto engineCatalogPath = engineRoot / ".rtr" / "catalog.json";

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "Phase12ParityPlugin" / "Content";
    const auto pluginArtifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto pluginCatalogPath = pluginRoot / ".rtr" / "catalog.json";

    const auto cookedRoot = FileSystem::GetCacheDir() / "Cooked";
    const auto cookedEngineArtifactPath = cookedRoot / "Engine" / "Defaults" / "Materials" / "Phase12ParityMaterial.json";
    const auto cookedPluginArtifactPath = cookedRoot / "Plugins" / "Phase12ParityPlugin" / "Materials" / "Checker.json";

    test_support::RemovePathIfExists(engineArtifactPath);
    test_support::RemovePathIfExists(engineCatalogPath);
    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
    test_support::RemoveTreeIfExists(cookedRoot);

    test_support::WriteTextFileOrFail(engineArtifactPath, "{\n  \"name\": \"engine-parity\"\n}\n");
    test_support::WriteTextFileOrFail(
        engineCatalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Engine/Defaults/Materials/Phase12ParityMaterial\",\n"
        "      \"sourceRelativePath\": \"Defaults/Materials/Phase12ParityMaterial.json\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Defaults/Materials/Phase12ParityMaterial.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    test_support::WriteTextFileOrFail(pluginArtifactPath, "{\n  \"name\": \"plugin-parity\"\n}\n");
    test_support::WriteTextFileOrFail(
        pluginCatalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/Phase12ParityPlugin/Materials/Checker\",\n"
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

    const auto sourceEngineResolved = FileSystem::ResolveReadPath("/Engine/Defaults/Materials/Phase12ParityMaterial");
    ASSERT_TRUE(sourceEngineResolved.has_value());
    EXPECT_EQ(*sourceEngineResolved, engineArtifactPath);

    const auto sourcePluginResolved = FileSystem::ResolveReadPath("/Plugins/Phase12ParityPlugin/Materials/Checker");
    ASSERT_TRUE(sourcePluginResolved.has_value());
    EXPECT_EQ(*sourcePluginResolved, pluginArtifactPath);

    test_support::WriteTextFileOrFail(
        cookedRoot / "Engine" / ".rtr" / "catalog.json",
        "{\n"
        "  \"version\": 2,\n"
        "  \"kind\": \"cooked\",\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Engine/Defaults/Materials/Phase12ParityMaterial\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Defaults/Materials/Phase12ParityMaterial.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"cooked\",\n"
        "          \"backendTag\": \"any\",\n"
        "          \"platformTag\": \"any\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");
    test_support::WriteTextFileOrFail(cookedEngineArtifactPath, "{\n  \"name\": \"engine-cooked\"\n}\n");

    test_support::WriteTextFileOrFail(
        cookedRoot / "Plugins" / "Phase12ParityPlugin" / ".rtr" / "catalog.json",
        "{\n"
        "  \"version\": 2,\n"
        "  \"kind\": \"cooked\",\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/Phase12ParityPlugin/Materials/Checker\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Materials/Checker.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"cooked\",\n"
        "          \"backendTag\": \"any\",\n"
        "          \"platformTag\": \"any\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");
    test_support::WriteTextFileOrFail(cookedPluginArtifactPath, "{\n  \"name\": \"plugin-cooked\"\n}\n");

    {
        ScopedEnvVar profileOverride("RTRLAB_RESOURCE_PROFILE", "cooked");
        FileSystem::RefreshCatalogs();

        const auto cookedEngineResolved = FileSystem::ResolveReadPath("/Engine/Defaults/Materials/Phase12ParityMaterial");
        ASSERT_TRUE(cookedEngineResolved.has_value());
        EXPECT_EQ(*cookedEngineResolved, cookedEngineArtifactPath);

        const auto cookedPluginResolved = FileSystem::ResolveReadPath("/Plugins/Phase12ParityPlugin/Materials/Checker");
        ASSERT_TRUE(cookedPluginResolved.has_value());
        EXPECT_EQ(*cookedPluginResolved, cookedPluginArtifactPath);
    }

    test_support::RemovePathIfExists(engineArtifactPath);
    test_support::RemovePathIfExists(engineCatalogPath);
    test_support::RemoveDirectoryIfEmpty(engineArtifactPath.parent_path());
    test_support::RemoveDirectoryIfEmpty(engineArtifactPath.parent_path().parent_path());
    test_support::RemoveDirectoryIfEmpty(engineCatalogPath.parent_path());
    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
    test_support::RemoveTreeIfExists(cookedRoot);
    FileSystem::RefreshCatalogs();
}

TEST(FileSystemContractTests, ResolveReadPathCanSwitchBetweenLooseAndPackagedArtifacts)
{
    FileSystem::Init();

    const auto engineRoot = FileSystem::GetRootPath() / "EngineContent";
    const auto engineArtifactPath = engineRoot / "Defaults" / "Materials" / "Phase13PackagedMaterial.json";
    const auto engineCatalogPath = engineRoot / ".rtr" / "catalog.json";

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "Phase13PackagedPlugin" / "Content";
    const auto pluginArtifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto pluginCatalogPath = pluginRoot / ".rtr" / "catalog.json";

    const auto cookedRoot = test_support::CurrentTestRoot("filesystem-contract") / "phase13-cooked";
    const auto packagedRoot = test_support::CurrentTestRoot("filesystem-contract") / "phase13-packaged";

    test_support::RemovePathIfExists(engineArtifactPath);
    test_support::RemovePathIfExists(engineCatalogPath);
    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
    test_support::RemoveTreeIfExists(cookedRoot);
    test_support::RemoveTreeIfExists(packagedRoot);

    test_support::WriteTextFileOrFail(engineArtifactPath, "{\n  \"name\": \"engine-packaged\"\n}\n");
    test_support::WriteTextFileOrFail(
        engineCatalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Engine/Defaults/Materials/Phase13PackagedMaterial\",\n"
        "      \"sourceRelativePath\": \"Defaults/Materials/Phase13PackagedMaterial.json\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Defaults/Materials/Phase13PackagedMaterial.json\",\n"
        "          \"format\": \"json\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    test_support::WriteTextFileOrFail(pluginArtifactPath, "{\n  \"name\": \"plugin-packaged\"\n}\n");
    test_support::WriteTextFileOrFail(
        pluginCatalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/Phase13PackagedPlugin/Materials/Checker\",\n"
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

    const auto sourceProjectResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(sourceProjectResolved.has_value());
    EXPECT_EQ(*sourceProjectResolved, FileSystem::GetAssetPath("textures/Grassy_Square.jpg"));

    const auto sourceEngineResolved = FileSystem::ResolveReadPath("/Engine/Defaults/Materials/Phase13PackagedMaterial");
    ASSERT_TRUE(sourceEngineResolved.has_value());
    EXPECT_EQ(*sourceEngineResolved, engineArtifactPath);

    const auto sourcePluginResolved = FileSystem::ResolveReadPath("/Plugins/Phase13PackagedPlugin/Materials/Checker");
    ASSERT_TRUE(sourcePluginResolved.has_value());
    EXPECT_EQ(*sourcePluginResolved, pluginArtifactPath);

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(FileSystem::GetRootPath(), "Content", &errorMessage)) << errorMessage;
    ASSERT_TRUE(Resource::CookRepositoryCatalogs(FileSystem::GetRootPath(), cookedRoot, "Content", &errorMessage)) << errorMessage;
    ASSERT_TRUE(Resource::PackageCookedRepositoryCatalogs(cookedRoot, packagedRoot, &errorMessage)) << errorMessage;

    {
        ScopedEnvVar profileOverride("RTRLAB_RESOURCE_PROFILE", "packaged");
        ScopedEnvVar packageRootOverride("RTRLAB_PACKAGE_ROOT", packagedRoot.string());
        FileSystem::RefreshCatalogs();

        const auto packagedProjectResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
        ASSERT_TRUE(packagedProjectResolved.has_value());
        EXPECT_NE(packagedProjectResolved->string().find("PackagedExtracted"), std::string::npos);
        EXPECT_EQ(packagedProjectResolved->extension().string(), ".rtrtex");

        const auto packagedProjectTexture = Resource::LoadCookedTexture(*packagedProjectResolved, &errorMessage);
        ASSERT_TRUE(packagedProjectTexture.has_value()) << errorMessage;

        const auto packagedEngineResolved = FileSystem::ResolveReadPath("/Engine/Defaults/Materials/Phase13PackagedMaterial");
        ASSERT_TRUE(packagedEngineResolved.has_value());
        EXPECT_NE(packagedEngineResolved->string().find("PackagedExtracted"), std::string::npos);
        const auto packagedEngineContents = FileSystem::ReadTextFile(*packagedEngineResolved);
        ASSERT_TRUE(packagedEngineContents.has_value());
        EXPECT_NE(packagedEngineContents->find("engine-packaged"), std::string::npos);

        const auto packagedPluginResolved = FileSystem::ResolveReadPath("/Plugins/Phase13PackagedPlugin/Materials/Checker");
        ASSERT_TRUE(packagedPluginResolved.has_value());
        EXPECT_NE(packagedPluginResolved->string().find("PackagedExtracted"), std::string::npos);
        const auto packagedPluginContents = FileSystem::ReadTextFile(*packagedPluginResolved);
        ASSERT_TRUE(packagedPluginContents.has_value());
        EXPECT_NE(packagedPluginContents->find("plugin-packaged"), std::string::npos);
    }

    test_support::RemovePathIfExists(engineArtifactPath);
    test_support::RemovePathIfExists(engineCatalogPath);
    test_support::RemoveDirectoryIfEmpty(engineArtifactPath.parent_path());
    test_support::RemoveDirectoryIfEmpty(engineArtifactPath.parent_path().parent_path());
    test_support::RemoveDirectoryIfEmpty(engineCatalogPath.parent_path());
    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
    test_support::RemoveTreeIfExists(cookedRoot);
    test_support::RemoveTreeIfExists(packagedRoot);
    test_support::RemoveTreeIfExists(FileSystem::GetCacheDir() / "PackagedExtracted");
    FileSystem::RefreshCatalogs();
}

TEST(FileSystemContractTests, ResolveReadPathCanLoadCookedProjectTextureArtifacts)
{
    FileSystem::Init();

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(FileSystem::GetRootPath(), "Content", &errorMessage)) << errorMessage;

    const auto cookedRoot = FileSystem::GetCacheDir() / "Cooked";
    test_support::RemoveTreeIfExists(cookedRoot);
    ASSERT_TRUE(Resource::CookRepositoryCatalogs(FileSystem::GetRootPath(), cookedRoot, "Content", &errorMessage)) << errorMessage;

    {
        ScopedEnvVar profileOverride("RTRLAB_RESOURCE_PROFILE", "cooked");
        FileSystem::RefreshCatalogs();

        const auto cookedResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
        ASSERT_TRUE(cookedResolved.has_value());
        EXPECT_EQ(cookedResolved->extension().string(), ".rtrtex");

        const auto cookedTexture = Resource::LoadCookedTexture(*cookedResolved, &errorMessage);
        ASSERT_TRUE(cookedTexture.has_value()) << errorMessage;
        EXPECT_GT(cookedTexture->width, 0u);
        EXPECT_GT(cookedTexture->height, 0u);
        EXPECT_EQ(cookedTexture->channelCount, 4u);
        EXPECT_EQ(cookedTexture->mipLevelCount, 1u);
        EXPECT_EQ(cookedTexture->rowPitch, cookedTexture->width * 4u);
        EXPECT_EQ(cookedTexture->pixelFormat, Resource::CookedTexturePixelFormat::RGBA8_UNorm);
        EXPECT_FALSE(cookedTexture->pixelData.empty());
    }

    test_support::RemoveTreeIfExists(cookedRoot);
    FileSystem::RefreshCatalogs();
}

TEST(FileSystemContractTests, ResolveReadPathUsesCatalogForExtensionlessEngineAsset)
{
    FileSystem::Init();

    const auto engineRoot = FileSystem::GetRootPath() / "EngineContent";
    const auto artifactPath = engineRoot / "Defaults" / "Materials" / "ErrorMaterial.json";
    const auto catalogPath = engineRoot / ".rtr" / "catalog.json";

    test_support::RemovePathIfExists(artifactPath);
    test_support::RemovePathIfExists(catalogPath);
    test_support::WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"error-material\"\n}\n");
    test_support::WriteTextFileOrFail(
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

    test_support::RemovePathIfExists(artifactPath);
    test_support::RemovePathIfExists(catalogPath);
    test_support::RemoveDirectoryIfEmpty(artifactPath.parent_path());
    test_support::RemoveDirectoryIfEmpty(artifactPath.parent_path().parent_path());
    test_support::RemoveDirectoryIfEmpty(catalogPath.parent_path());
}

TEST(FileSystemContractTests, ResolveReadPathUsesCatalogForExtensionlessPluginAsset)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "CatalogTestPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"checker\"\n}\n");
    test_support::WriteTextFileOrFail(
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveReadPathRejectsPluginPathsWithInvalidMountNames)
{
    FileSystem::Init();

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/Bad-Plugin/Materials/Checker").has_value());
    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/1BadPlugin/Materials/Checker").has_value());
}

TEST(FileSystemContractTests, ResolveReadPathKeepsProjectAndPluginNamespacesSeparate)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "ShadowPlugin" / "Content";
    const auto pluginArtifactPath = pluginRoot / "Textures" / "Grassy_Square.jpg";
    const auto pluginCatalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(pluginArtifactPath, "plugin-shadow");
    test_support::WriteTextFileOrFail(
        pluginCatalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/ShadowPlugin/Textures/Grassy_Square\",\n"
        "      \"sourceRelativePath\": \"Textures/Grassy_Square.jpg\",\n"
        "      \"artifacts\": [\n"
        "        {\n"
        "          \"relativePath\": \"Textures/Grassy_Square.jpg\",\n"
        "          \"format\": \"jpg\",\n"
        "          \"profileTag\": \"dev\"\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n");

    FileSystem::RefreshCatalogs();

    const auto projectResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
    const auto pluginResolved = FileSystem::ResolveReadPath("/Plugins/ShadowPlugin/Textures/Grassy_Square");

    ASSERT_TRUE(projectResolved.has_value());
    ASSERT_TRUE(pluginResolved.has_value());
    EXPECT_EQ(*projectResolved, FileSystem::GetAssetPath("textures/Grassy_Square.jpg"));
    EXPECT_EQ(*pluginResolved, pluginArtifactPath);

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
    FileSystem::RefreshCatalogs();
}

TEST(FileSystemContractTests, ResolveReadPathPrefersMostSpecificArtifactForCurrentRuntime)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "RuntimeSpecificCatalogPlugin" / "Content";
    const auto fallbackPath = pluginRoot / "Materials" / "Fallback.json";
    const auto backendSpecificPath = pluginRoot / "Materials" / "BackendSpecific.json";
    const auto runtimeSpecificPath = pluginRoot / "Materials" / "RuntimeSpecific.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(fallbackPath, "{\n  \"name\": \"fallback\"\n}\n");
    test_support::WriteTextFileOrFail(backendSpecificPath, "{\n  \"name\": \"backend\"\n}\n");
    test_support::WriteTextFileOrFail(runtimeSpecificPath, "{\n  \"name\": \"runtime\"\n}\n");
    test_support::WriteTextFileOrFail(
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
            "          \"backendTag\": \"" +
            std::string(CurrentBackendTag()) + "\",\n"
                                               "          \"platformTag\": \"any\"\n"
                                               "        },\n"
                                               "        {\n"
                                               "          \"relativePath\": \"Materials/RuntimeSpecific.json\",\n"
                                               "          \"format\": \"json\",\n"
                                               "          \"profileTag\": \"" +
            std::string(CurrentProfileTag()) + "\",\n"
                                               "          \"backendTag\": \"" +
            std::string(CurrentBackendTag()) + "\",\n"
                                               "          \"platformTag\": \"" +
            std::string(CurrentPlatformTag()) + "\"\n"
                                                "        }\n"
                                                "      ]\n"
                                                "    }\n"
                                                "  ]\n"
                                                "}\n");

    const auto resolved = FileSystem::ResolveReadPath("/Plugins/RuntimeSpecificCatalogPlugin/Materials/Picker");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, runtimeSpecificPath);

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveReadPathFallsBackToAnyArtifactWhenSpecificTagsDoNotMatch)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "FallbackCatalogPlugin" / "Content";
    const auto fallbackPath = pluginRoot / "Materials" / "Fallback.json";
    const auto mismatchedPath = pluginRoot / "Materials" / "Mismatched.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(fallbackPath, "{\n  \"name\": \"fallback\"\n}\n");
    test_support::WriteTextFileOrFail(mismatchedPath, "{\n  \"name\": \"mismatch\"\n}\n");
    test_support::WriteTextFileOrFail(
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveWritePathMapsSavedConfigLogicalPathToSavedConfigDirectory)
{
    FileSystem::Init();

    const auto resolved = FileSystem::ResolveWritePath("/Saved/Config/test-contract/Phase12Config.json");

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, FileSystem::GetSavedConfigPath("test-contract/Phase12Config.json"));
    EXPECT_TRUE(std::filesystem::exists(resolved->parent_path()));

    test_support::RemoveTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
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

    test_support::RemoveTreeIfExists(FileSystem::GetCacheDir() / "Shaders" / "opengl");
    test_support::RemoveDirectoryIfEmpty(FileSystem::GetCacheDir() / "Shaders");
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveReadPathRejectsSourceCatalogEntriesWithoutSourceRelativePath)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "MissingSourcePathPlugin" / "Content";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(
        catalogPath,
        "{\n"
        "  \"version\": 1,\n"
        "  \"entries\": [\n"
        "    {\n"
        "      \"logicalPath\": \"/Plugins/MissingSourcePathPlugin/Materials/Checker\",\n"
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

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/MissingSourcePathPlugin/Materials/Checker").has_value());

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, RefreshCatalogsDiscoversPluginCatalogAddedAfterInitialLookup)
{
    FileSystem::Init();

    const auto initialProjectResolved = FileSystem::ResolveReadPath("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(initialProjectResolved.has_value());

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "LateBoundCatalogPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"late-bound\"\n}\n");
    test_support::WriteTextFileOrFail(
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
}

TEST(FileSystemContractTests, RefreshCatalogsDropsPluginCatalogRemovedAfterInitialLookup)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "RemovedCatalogPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"removable\"\n}\n");
    test_support::WriteTextFileOrFail(
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    FileSystem::RefreshCatalogs();

    EXPECT_FALSE(FileSystem::ResolveReadPath("/Plugins/RemovedCatalogPlugin/Materials/Checker").has_value());
}

TEST(FileSystemContractTests, ResolveReadPathRejectsCatalogWithDuplicateLogicalPaths)
{
    FileSystem::Init();

    const auto pluginRoot = FileSystem::GetRootPath() / "Plugins" / "DuplicateCatalogPlugin" / "Content";
    const auto artifactPath = pluginRoot / "Materials" / "Checker.json";
    const auto catalogPath = pluginRoot / ".rtr" / "catalog.json";

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());

    test_support::WriteTextFileOrFail(artifactPath, "{\n  \"name\": \"checker\"\n}\n");
    test_support::WriteTextFileOrFail(
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

    test_support::RemoveTreeIfExists(pluginRoot.parent_path().parent_path());
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

    test_support::RemovePathIfExists(savedPath);

    ASSERT_TRUE(FileSystem::WriteText(kVirtualPath, kExpectedContents));
    const auto contents = FileSystem::ReadText(kVirtualPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_EQ(*contents, kExpectedContents);

    test_support::RemovePathIfExists(savedPath);
    test_support::RemoveTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
}

TEST(FileSystemContractTests, WriteBinarySupportsCacheLogicalPaths)
{
    FileSystem::Init();

    constexpr std::string_view kVirtualPath = "/Cache/Shaders/test-contract/ForwardLit.bin";
    const std::vector<uint8_t> expectedData{0x10, 0x20, 0x30, 0x40};
    const auto cachePath = FileSystem::GetCacheDir() / "Shaders" / "test-contract" / "ForwardLit.bin";

    test_support::RemovePathIfExists(cachePath);

    ASSERT_TRUE(FileSystem::WriteBinary(kVirtualPath, expectedData));
    const auto contents = FileSystem::ReadBinary(kVirtualPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_EQ(*contents, expectedData);

    test_support::RemovePathIfExists(cachePath);
    test_support::RemoveTreeIfExists(FileSystem::GetCacheDir() / "Shaders" / "test-contract");
    test_support::RemoveDirectoryIfEmpty(FileSystem::GetCacheDir() / "Shaders");
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
    test_support::RemovePathIfExists(savedPath);
    ASSERT_TRUE(FileSystem::WriteText("/Saved/Config/input/DebugCameraControl.json", "{\n  \"source\": \"saved\"\n}\n"));

    const auto resolved = FileSystem::ResolveConfigPath(kExistingConfig);

    EXPECT_EQ(resolved, savedPath);
    EXPECT_TRUE(std::filesystem::exists(resolved));

    test_support::RemovePathIfExists(savedPath);
    test_support::RemoveDirectoryIfEmpty(savedPath.parent_path());
    test_support::RemoveDirectoryIfEmpty(savedPath.parent_path().parent_path());
}

TEST(FileSystemContractTests, ResolveConfigPathCopiesDefaultConfigIntoSavedDirectory)
{
    FileSystem::Init();

    const auto assetPath = FileSystem::GetAssetPath("Config") / kTempConfig;
    const auto savedPath = FileSystem::GetSavedConfigPath(kTempConfig);
    const std::string expectedContents = "{\n  \"source\": \"contract-test\"\n}\n";

    test_support::RemovePathIfExists(savedPath);
    test_support::RemovePathIfExists(assetPath);
    test_support::WriteTextFileOrFail(assetPath, expectedContents);

    const auto resolved = FileSystem::ResolveConfigPath(kTempConfig);

    EXPECT_EQ(resolved, savedPath);
    ASSERT_TRUE(std::filesystem::exists(savedPath));
    const auto contents = FileSystem::ReadTextFile(savedPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_EQ(*contents, expectedContents);

    test_support::RemovePathIfExists(savedPath);
    test_support::RemovePathIfExists(assetPath);
    test_support::RemoveTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
    test_support::RemoveDirectoryIfEmpty(assetPath.parent_path());
}

TEST(FileSystemContractTests, ResolveConfigPathReturnsEmptyWhenConfigIsMissing)
{
    FileSystem::Init();

    const auto assetPath = FileSystem::GetAssetPath("Config") / "test-contract/MissingConfig.json";
    const auto savedPath = FileSystem::GetSavedConfigPath("test-contract/MissingConfig.json");

    test_support::RemovePathIfExists(savedPath);
    test_support::RemovePathIfExists(assetPath);

    const auto resolved = FileSystem::ResolveConfigPath("test-contract/MissingConfig.json");

    EXPECT_TRUE(resolved.empty());

    test_support::RemovePathIfExists(savedPath);
    test_support::RemovePathIfExists(assetPath);
    test_support::RemoveTreeIfExists(FileSystem::GetSavedDir() / "Config" / "test-contract");
    test_support::RemoveDirectoryIfEmpty(assetPath.parent_path());
}

TEST(FileSystemContractTests, ResolveConfigPathFallsBackToEngineDefaultWhenProjectDefaultIsMissing)
{
    FileSystem::Init();

    constexpr std::string_view kRelativePath = "input/DefaultBindings.json";
    const auto projectPath = FileSystem::GetAssetPath("Config") / kRelativePath;
    const auto savedPath = FileSystem::GetSavedConfigPath(kRelativePath);
    const auto enginePath = FileSystem::GetRootPath() / "EngineContent" / "Config" / kRelativePath;

    ASSERT_TRUE(std::filesystem::exists(enginePath));

    test_support::RemovePathIfExists(savedPath);
    test_support::RemovePathIfExists(projectPath);

    const auto resolved = FileSystem::ResolveConfigPath(kRelativePath);

    EXPECT_EQ(resolved, savedPath);
    ASSERT_TRUE(std::filesystem::exists(savedPath));
    const auto contents = FileSystem::ReadTextFile(savedPath);
    ASSERT_TRUE(contents.has_value());
    EXPECT_NE(contents->find("\"engine-default\""), std::string::npos);

    test_support::RemovePathIfExists(savedPath);
    test_support::RemoveDirectoryIfEmpty(savedPath.parent_path());
    test_support::RemoveDirectoryIfEmpty(savedPath.parent_path().parent_path());
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
