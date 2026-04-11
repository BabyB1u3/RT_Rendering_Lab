#include <gtest/gtest.h>

#include <filesystem>
#include <iterator>
#include <string>

#include "Core/Resource/Package/PakArchive.h"
#include "ResourceTestSupport.h"

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

TEST_F(PakArchiveTests, BuildPakArchiveCanReadEntries)
{
    const auto sourceRoot = TestRoot() / "Mount";
    const auto packagedRoot = TestRoot() / "out";
    const auto pakPath = test_support::GamePackagedArchivePath(packagedRoot);
    test_support::WriteTextFileOrFail(test_support::MountCatalogPath(sourceRoot), "{\n  \"version\": 2\n}\n");
    test_support::WriteMountFileOrFail(sourceRoot, "Textures/Checker.txt", "checker");

    std::string errorMessage;
    ASSERT_TRUE(Resource::BuildPakArchive(sourceRoot, pakPath, &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(pakPath, ".rtr/catalog.json", &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(pakPath, "Textures/Checker.txt", &errorMessage)) << errorMessage;

    const auto bytes = Resource::ReadPakEntry(pakPath, "Textures/Checker.txt", &errorMessage);
    ASSERT_TRUE(bytes.has_value()) << errorMessage;
    const std::string text(bytes->begin(), bytes->end());
    EXPECT_EQ(text, "checker");
}

TEST_F(PakArchiveTests, PackageCookedRepositoryCatalogsWritesMergedGameArchive)
{
    const auto cookedRoot = TestRoot() / "Cooked";
    const auto packagedRoot = TestRoot() / "Packaged";

    test_support::WriteTextFileOrFail(
        test_support::ProjectCookedCatalogPath(cookedRoot),
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteProjectCookedFileOrFail(cookedRoot, "Textures/Grassy_Square.rtrtex", "project");

    test_support::WriteTextFileOrFail(
        test_support::EngineCookedCatalogPath(cookedRoot),
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": []\n}\n");
    test_support::WriteEngineCookedFileOrFail(cookedRoot, "Defaults/Materials/ErrorMaterial.json", "engine");

    std::string errorMessage;
    ASSERT_TRUE(Resource::PackageCookedRepositoryCatalogs(cookedRoot, packagedRoot, &errorMessage)) << errorMessage;

    const auto gamePak = test_support::GamePackagedArchivePath(packagedRoot);

    EXPECT_TRUE(std::filesystem::exists(gamePak));

    EXPECT_TRUE(Resource::PakEntryExists(gamePak, ".rtr/catalog.json", &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(gamePak, "Project/Textures/Grassy_Square.rtrtex", &errorMessage)) << errorMessage;
    EXPECT_TRUE(Resource::PakEntryExists(gamePak, "Engine/Defaults/Materials/ErrorMaterial.json", &errorMessage)) << errorMessage;

    const auto catalogBytes = Resource::ReadPakEntry(gamePak, ".rtr/catalog.json", &errorMessage);
    ASSERT_TRUE(catalogBytes.has_value()) << errorMessage;
    const std::string catalogText(catalogBytes->begin(), catalogBytes->end());
    EXPECT_NE(catalogText.find("\"logicalPath\": \"/Project/"), std::string::npos);
    EXPECT_NE(catalogText.find("\"logicalPath\": \"/Engine/"), std::string::npos);
}
