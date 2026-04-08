#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

#include "Core/Resource/CookedCatalog.h"
#include "Core/Resource/SourceCatalog.h"
#include "TestPaths.h"

namespace
{
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

    struct BootstrapTextureHeader
    {
        char magic[8];
        uint32_t version;
        uint32_t width;
        uint32_t height;
        uint32_t channelCount;
        uint32_t pixelFormat;
        uint32_t dataSize;
    };

    static_assert(sizeof(BootstrapTextureHeader) == 32);
} // namespace

TEST_F(CookedCatalogTests, CookRepositoryCatalogsCopiesArtifactsAndWritesCookedCatalog)
{
    const auto repoRoot = TestRoot() / "Repo";
    const std::vector<unsigned char> kOnePixelPng{
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
        0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
        0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x89, 0x99, 0x3D, 0x1D, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    };
    test_support::WriteBinaryFileOrFail(repoRoot / "Content" / "textures" / "Grassy_Square.png", kOnePixelPng);

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(repoRoot, "Content", &errorMessage)) << errorMessage;

    const auto cookedRoot = repoRoot / "Saved" / "Cache" / "Cooked";
    ASSERT_TRUE(Resource::CookRepositoryCatalogs(repoRoot, cookedRoot, "Content", &errorMessage)) << errorMessage;

    const auto cookedCatalogPath = cookedRoot / "Project" / ".rtr" / "catalog.json";
    const auto cookedArtifactPath = cookedRoot / "Project" / "Textures" / "Grassy_Square.ktx2";

    EXPECT_TRUE(std::filesystem::exists(cookedCatalogPath));
    EXPECT_TRUE(std::filesystem::exists(cookedArtifactPath));

    std::ifstream cookedCatalog(cookedCatalogPath);
    ASSERT_TRUE(cookedCatalog.is_open());
    const std::string cookedContents((std::istreambuf_iterator<char>(cookedCatalog)), std::istreambuf_iterator<char>());

    EXPECT_NE(cookedContents.find("/Project/Textures/Grassy_Square"), std::string::npos);
    EXPECT_NE(cookedContents.find("\"profileTag\": \"cooked\""), std::string::npos);
    EXPECT_NE(cookedContents.find("\"relativePath\": \"Textures/Grassy_Square.ktx2\""), std::string::npos);
    EXPECT_NE(cookedContents.find("\"format\": \"ktx2\""), std::string::npos);

    std::ifstream cookedArtifact(cookedArtifactPath, std::ios::binary);
    ASSERT_TRUE(cookedArtifact.is_open());
    BootstrapTextureHeader header{};
    cookedArtifact.read(reinterpret_cast<char *>(&header), sizeof(header));
    ASSERT_TRUE(cookedArtifact.good());
    EXPECT_EQ(std::string(header.magic, header.magic + 8), "RTRTEX01");
    EXPECT_EQ(header.version, 1u);
    EXPECT_EQ(header.width, 1u);
    EXPECT_EQ(header.height, 1u);
    EXPECT_EQ(header.channelCount, 4u);
    EXPECT_EQ(header.pixelFormat, 1u);
    EXPECT_EQ(header.dataSize, 4u);

    std::vector<unsigned char> pixelData(header.dataSize);
    cookedArtifact.read(reinterpret_cast<char *>(pixelData.data()), static_cast<std::streamsize>(pixelData.size()));
    ASSERT_TRUE(cookedArtifact.good() || cookedArtifact.eof());
    EXPECT_EQ(pixelData.size(), 4u);
}
