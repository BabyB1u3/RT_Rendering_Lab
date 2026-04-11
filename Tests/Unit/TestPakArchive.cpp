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

