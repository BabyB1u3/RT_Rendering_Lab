#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

#include "Core/Resource/Catalog/SourceCatalog.h"
#include "Core/Resource/Cook/CookedCatalog.h"
#include "ResourceTestSupport.h"

namespace
{
    const std::vector<unsigned char> kOnePixelPng{
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
        0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
        0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x89, 0x99, 0x3D, 0x1D, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    };

    struct LegacyBootstrapTextureHeader
    {
        char magic[8] = {'R', 'T', 'R', 'T', 'E', 'X', '0', '1'};
        uint32_t version = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channelCount = 4;
        uint32_t pixelFormat = 1;
        uint32_t dataSize = 0;
    };

    class CookedCatalogTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("cooked-catalog");
            test_support::ResetCurrentTestRoot("cooked-catalog");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("cooked-catalog");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }
    private:
        std::filesystem::path m_TestRoot;
    };

} // namespace

TEST_F(CookedCatalogTests, CookRepositoryCatalogsCopiesArtifactsAndWritesCookedCatalog)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteProjectBinaryFileOrFail(repoRoot, "textures/Grassy_Square.png", kOnePixelPng);
    EXPECT_FALSE(std::filesystem::exists(test_support::ProjectSourceCatalogPath(repoRoot)));

    std::string errorMessage;
    const auto cookedRoot = test_support::CookedRoot(repoRoot);
    ASSERT_TRUE(Resource::CookRepositoryCatalogs(repoRoot, cookedRoot, "Project", &errorMessage)) << errorMessage;

    const auto cookedCatalogPath = test_support::ProjectCookedCatalogPath(cookedRoot);
    const auto cookedArtifactPath = test_support::ProjectCookedRoot(cookedRoot) / "Textures" / "Grassy_Square.rtrtex";

    EXPECT_TRUE(std::filesystem::exists(cookedCatalogPath));
    EXPECT_TRUE(std::filesystem::exists(cookedArtifactPath));

    std::ifstream cookedCatalog(cookedCatalogPath);
    ASSERT_TRUE(cookedCatalog.is_open());
    const std::string cookedContents((std::istreambuf_iterator<char>(cookedCatalog)), std::istreambuf_iterator<char>());

    EXPECT_NE(cookedContents.find("\"version\": 2"), std::string::npos);
    EXPECT_NE(cookedContents.find("\"kind\": \"cooked\""), std::string::npos);
    EXPECT_NE(cookedContents.find("/Project/Textures/Grassy_Square"), std::string::npos);
    EXPECT_NE(cookedContents.find("\"profileTag\": \"cooked\""), std::string::npos);
    EXPECT_NE(cookedContents.find("\"relativePath\": \"Textures/Grassy_Square.rtrtex\""), std::string::npos);
    EXPECT_NE(cookedContents.find("\"format\": \"rtrtex\""), std::string::npos);
    EXPECT_EQ(cookedContents.find("sourceRelativePath"), std::string::npos);

    std::string errorMessageFromLoad;
    const auto metadata = Resource::ReadCookedTextureMetadata(cookedArtifactPath, &errorMessageFromLoad);
    ASSERT_TRUE(metadata.has_value()) << errorMessageFromLoad;
    EXPECT_EQ(metadata->width, 1u);
    EXPECT_EQ(metadata->height, 1u);
    EXPECT_EQ(metadata->channelCount, 4u);
    EXPECT_EQ(metadata->mipLevelCount, 1u);
    EXPECT_EQ(metadata->rowPitch, 4u);
    EXPECT_EQ(metadata->dataSize, 4u);
    EXPECT_EQ(metadata->pixelFormat, Resource::CookedTexturePixelFormat::RGBA8_UNorm);

    const auto cookedTexture = Resource::LoadCookedTexture(cookedArtifactPath, &errorMessageFromLoad);
    ASSERT_TRUE(cookedTexture.has_value()) << errorMessageFromLoad;
    EXPECT_EQ(cookedTexture->width, 1u);
    EXPECT_EQ(cookedTexture->height, 1u);
    EXPECT_EQ(cookedTexture->channelCount, 4u);
    EXPECT_EQ(cookedTexture->mipLevelCount, 1u);
    EXPECT_EQ(cookedTexture->rowPitch, 4u);
    EXPECT_EQ(cookedTexture->pixelFormat, Resource::CookedTexturePixelFormat::RGBA8_UNorm);
    EXPECT_EQ(cookedTexture->pixelData.size(), 4u);
}

TEST_F(CookedCatalogTests, CookRepositoryCatalogsPreservesLogicalPathsAcrossProjectAndEngineMounts)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    test_support::WriteProjectBinaryFileOrFail(repoRoot, "textures/Grassy_Square.png", kOnePixelPng);
    test_support::WriteEngineFileOrFail(
        repoRoot,
        "Defaults/Materials/ErrorMaterial.json",
        "{\n  \"name\": \"error-material\"\n}\n");
    EXPECT_FALSE(std::filesystem::exists(test_support::ProjectSourceCatalogPath(repoRoot)));
    EXPECT_FALSE(std::filesystem::exists(test_support::EngineSourceCatalogPath(repoRoot)));

    std::string errorMessage;
    const auto cookedRoot = test_support::CookedRoot(repoRoot);
    ASSERT_TRUE(Resource::CookRepositoryCatalogs(repoRoot, cookedRoot, "Project", &errorMessage)) << errorMessage;

    const auto readFile = [](const std::filesystem::path &path) {
        std::ifstream in(path);
        EXPECT_TRUE(in.is_open()) << path.string();
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };

    const auto projectCookedCatalog = readFile(test_support::ProjectCookedCatalogPath(cookedRoot));
    const auto engineCookedCatalog = readFile(test_support::EngineCookedCatalogPath(cookedRoot));

    EXPECT_NE(projectCookedCatalog.find("/Project/Textures/Grassy_Square"), std::string::npos);
    EXPECT_NE(engineCookedCatalog.find("/Engine/Defaults/Materials/ErrorMaterial"), std::string::npos);

    EXPECT_TRUE(std::filesystem::exists(test_support::ProjectCookedRoot(cookedRoot) / "Textures" / "Grassy_Square.rtrtex"));
    EXPECT_TRUE(std::filesystem::exists(test_support::EngineCookedRoot(cookedRoot) / "Defaults" / "Materials" / "ErrorMaterial.json"));
}

TEST_F(CookedCatalogTests, LoadCookedTextureRejectsInvalidMagic)
{
    const auto cookedArtifactPath = TestRoot() / "invalid.rtrtex";
    std::vector<unsigned char> invalidHeader(32, 0);
    invalidHeader[0] = 'B';
    invalidHeader[1] = 'A';
    invalidHeader[2] = 'D';
    invalidHeader[3] = 'T';
    invalidHeader[4] = 'E';
    invalidHeader[5] = 'X';
    invalidHeader[6] = '0';
    invalidHeader[7] = '1';
    test_support::WriteBinaryFileOrFail(cookedArtifactPath, invalidHeader);

    std::string errorMessage;
    const auto cookedTexture = Resource::LoadCookedTexture(cookedArtifactPath, &errorMessage);

    EXPECT_FALSE(cookedTexture.has_value());
    EXPECT_NE(errorMessage.find("invalid magic"), std::string::npos);
}

TEST_F(CookedCatalogTests, GetCookOutputRootSupportsCacheAndBuildLayouts)
{
    const auto repoRoot = TestRoot() / "Repo";

    EXPECT_EQ(Resource::GetCookOutputRoot(repoRoot, Resource::CookOutputLayout::Cache),
              test_support::CookedRoot(repoRoot));
    EXPECT_EQ(Resource::GetCookOutputRoot(repoRoot, Resource::CookOutputLayout::Build),
              test_support::BuildCookedRoot(repoRoot));
}

TEST_F(CookedCatalogTests, LoadCookedTextureSupportsLegacyVersion1Artifacts)
{
    const auto cookedArtifactPath = TestRoot() / "legacy.rtrtex";
    const LegacyBootstrapTextureHeader header{
        .version = 1,
        .width = 1,
        .height = 1,
        .channelCount = 4,
        .pixelFormat = static_cast<uint32_t>(Resource::CookedTexturePixelFormat::RGBA8_UNorm),
        .dataSize = 4,
    };

    std::vector<unsigned char> bytes(sizeof(header) + 4, 0);
    std::memcpy(bytes.data(), &header, sizeof(header));
    bytes[sizeof(header) + 0] = 255;
    bytes[sizeof(header) + 1] = 128;
    bytes[sizeof(header) + 2] = 64;
    bytes[sizeof(header) + 3] = 32;
    test_support::WriteBinaryFileOrFail(cookedArtifactPath, bytes);

    std::string errorMessage;
    const auto metadata = Resource::ReadCookedTextureMetadata(cookedArtifactPath, &errorMessage);
    ASSERT_TRUE(metadata.has_value()) << errorMessage;
    EXPECT_EQ(metadata->mipLevelCount, 1u);
    EXPECT_EQ(metadata->rowPitch, 4u);

    const auto cookedTexture = Resource::LoadCookedTexture(cookedArtifactPath, &errorMessage);
    ASSERT_TRUE(cookedTexture.has_value()) << errorMessage;
    EXPECT_EQ(cookedTexture->width, 1u);
    EXPECT_EQ(cookedTexture->height, 1u);
    EXPECT_EQ(cookedTexture->channelCount, 4u);
    EXPECT_EQ(cookedTexture->mipLevelCount, 1u);
    EXPECT_EQ(cookedTexture->rowPitch, 4u);
    ASSERT_EQ(cookedTexture->pixelData.size(), 4u);
    EXPECT_EQ(cookedTexture->pixelData[0], 255u);
    EXPECT_EQ(cookedTexture->pixelData[1], 128u);
    EXPECT_EQ(cookedTexture->pixelData[2], 64u);
    EXPECT_EQ(cookedTexture->pixelData[3], 32u);
}
