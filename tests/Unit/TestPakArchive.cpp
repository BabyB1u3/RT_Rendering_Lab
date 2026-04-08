#include <gtest/gtest.h>

#include <filesystem>
#include <iterator>
#include <string>

#include "Core/Resource/PakArchive.h"
#include "TestPaths.h"

namespace
{
    class PakArchiveTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("pak-archive");
            test_support::ResetCurrentTestRoot("pak-archive");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("pak-archive");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(PakArchiveTests, BuildPakArchiveCanReadAndMaterializeEntries)
{
    const auto sourceRoot = TestRoot() / "Mount";
    const auto pakPath = TestRoot() / "out" / "Project.rtrpak";
    test_support::WriteTextFileOrFail(sourceRoot / ".rtr" / "catalog.json", "{\n  \"version\": 2\n}\n");
    test_support::WriteTextFileOrFail(sourceRoot / "Textures" / "Checker.txt", "checker");

    std::string errorMessage;
    ASSERT_TRUE(Resource::BuildPakArchive(sourceRoot, pakPath, &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(pakPath, ".rtr/catalog.json", &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(pakPath, "Textures/Checker.txt", &errorMessage)) << errorMessage;

    const auto bytes = Resource::ReadPakEntry(pakPath, "Textures/Checker.txt", &errorMessage);
    ASSERT_TRUE(bytes.has_value()) << errorMessage;
    const std::string text(bytes->begin(), bytes->end());
    EXPECT_EQ(text, "checker");

    const auto materialized = Resource::MaterializePakEntry(
        pakPath, "Textures/Checker.txt", TestRoot() / "extracted" / "Project", &errorMessage);
    ASSERT_TRUE(materialized.has_value()) << errorMessage;

    std::ifstream in(*materialized);
    ASSERT_TRUE(in.is_open());
    const std::string materializedText((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(materializedText, "checker");
}

TEST_F(PakArchiveTests, PackageCookedRepositoryCatalogsWritesProjectEngineAndPluginArchives)
{
    const auto cookedRoot = TestRoot() / "Cooked";
    const auto packagedRoot = TestRoot() / "Packaged";

    test_support::WriteTextFileOrFail(
        cookedRoot / "Project" / ".rtr" / "catalog.json",
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteTextFileOrFail(cookedRoot / "Project" / "Textures" / "Grassy_Square.rtrtex", "project");

    test_support::WriteTextFileOrFail(
        cookedRoot / "Engine" / ".rtr" / "catalog.json",
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteTextFileOrFail(cookedRoot / "Engine" / "Defaults" / "Materials" / "ErrorMaterial.json", "engine");

    test_support::WriteTextFileOrFail(
        cookedRoot / "Plugins" / "ExamplePlugin" / ".rtr" / "catalog.json",
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteTextFileOrFail(cookedRoot / "Plugins" / "ExamplePlugin" / "Materials" / "Checker.json", "plugin");

    std::string errorMessage;
    ASSERT_TRUE(Resource::PackageCookedRepositoryCatalogs(cookedRoot, packagedRoot, &errorMessage)) << errorMessage;

    const auto projectPak = packagedRoot / "Project.rtrpak";
    const auto enginePak = packagedRoot / "Engine.rtrpak";
    const auto pluginPak = packagedRoot / "Plugins" / "ExamplePlugin.rtrpak";

    EXPECT_TRUE(std::filesystem::exists(projectPak));
    EXPECT_TRUE(std::filesystem::exists(enginePak));
    EXPECT_TRUE(std::filesystem::exists(pluginPak));

    EXPECT_TRUE(Resource::PakEntryExists(projectPak, ".rtr/catalog.json", &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(enginePak, ".rtr/catalog.json", &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(pluginPak, ".rtr/catalog.json", &errorMessage)) << errorMessage;
}
