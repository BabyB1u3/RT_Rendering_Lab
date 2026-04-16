#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "Core/Resource/Cook/CookedCatalog.h"
#include "ResourceTestSupport.h"

namespace
{
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

    void TearDown() override { test_support::RemoveCurrentTestArtifacts("cooked-catalog"); }

    std::filesystem::path TestRoot() const { return m_TestRoot; }

private:
    std::filesystem::path m_TestRoot;
};

} // namespace

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

TEST_F(CookedCatalogTests, LoadCookedTextureSupportsLegacyVersion1Artifacts)
{
    const auto cookedArtifactPath = TestRoot() / "legacy.rtrtex";
    const LegacyBootstrapTextureHeader header{
        .version = 1,
        .width = 1,
        .height = 1,
        .channelCount = 4,
        .pixelFormat = static_cast<uint32_t>(Resource::CookedTexturePixelFormat::RGBA8_UNORM),
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
    EXPECT_EQ(metadata->m_MipLevelCount, 1u);
    EXPECT_EQ(metadata->m_RowPitch, 4u);

    const auto cookedTexture = Resource::LoadCookedTexture(cookedArtifactPath, &errorMessage);
    ASSERT_TRUE(cookedTexture.has_value()) << errorMessage;
    EXPECT_EQ(cookedTexture->m_Width, 1u);
    EXPECT_EQ(cookedTexture->m_Height, 1u);
    EXPECT_EQ(cookedTexture->m_ChannelCount, 4u);
    EXPECT_EQ(cookedTexture->m_MipLevelCount, 1u);
    EXPECT_EQ(cookedTexture->m_RowPitch, 4u);
    ASSERT_EQ(cookedTexture->m_PixelData.size(), 4u);
    EXPECT_EQ(cookedTexture->m_PixelData[0], 255u);
    EXPECT_EQ(cookedTexture->m_PixelData[1], 128u);
    EXPECT_EQ(cookedTexture->m_PixelData[2], 64u);
    EXPECT_EQ(cookedTexture->m_PixelData[3], 32u);
}
