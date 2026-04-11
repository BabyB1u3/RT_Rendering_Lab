#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "Core/Resource/Package/PakArchive.h"
#include "ResourceTestSupport.h"

namespace
{
    class PackagePipelineTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("package-pipeline");
            test_support::ResetCurrentTestRoot("package-pipeline");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("package-pipeline");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(PackagePipelineTests, PackageCookedRepositoryCatalogsWritesMergedGameArchive)
{
    const auto cookedRoot = TestRoot() / "Cooked";
    const auto packagedRoot = TestRoot() / "Packaged";

    test_support::WriteTextFileOrFail(
        test_support::ProjectCookedCatalogPath(cookedRoot),
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": \"/Project/Textures/Grassy_Square\",\n      \"artifacts\": [\n        {\n          \"relativePath\": \"Textures/Grassy_Square.rtrtex\",\n          \"format\": \"rtrtex\",\n          \"profileTag\": \"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    }\n  ]\n}\n");
    test_support::WriteProjectCookedFileOrFail(cookedRoot, "Textures/Grassy_Square.rtrtex", "project");

    test_support::WriteTextFileOrFail(
        test_support::EngineCookedCatalogPath(cookedRoot),
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": \"/Engine/Defaults/Materials/ErrorMaterial\",\n      \"artifacts\": [\n        {\n          \"relativePath\": \"Defaults/Materials/ErrorMaterial.json\",\n          \"format\": \"json\",\n          \"profileTag\": \"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    }\n  ]\n}\n");
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
    EXPECT_NE(catalogText.find("\"relativePath\": \"Project/Textures/Grassy_Square.rtrtex\""), std::string::npos);
    EXPECT_NE(catalogText.find("\"relativePath\": \"Engine/Defaults/Materials/ErrorMaterial.json\""), std::string::npos);
}
