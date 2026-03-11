#include <gtest/gtest.h>
#include <memory>
#include <array>

#include "GlTestContext.h"
#include "graphics/Texture.h"

class TextureIntegrationTests : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        s_Context = CreateScope<GlTestContext>();
    }

    static void TearDownTestSuite()
    {
        s_Context.reset();
    }

    inline static Scope<GlTestContext> s_Context;
};

TEST_F(TextureIntegrationTests, CreateTextureFromSpecification)
{
    TextureSpecification spec{};
    spec.Width = 2;
    spec.Height = 2;
    spec.Format = TextureFormat::RGBA8;

    auto texture = Texture2D::Create(spec);

    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->GetWidth(), 2u);
    EXPECT_EQ(texture->GetHeight(), 2u);
    EXPECT_EQ(texture->GetFormat(), TextureFormat::RGBA8);
    EXPECT_NE(texture->GetRendererID(), 0u);
}

TEST_F(TextureIntegrationTests, SetDataDoesNotCrash)
{
    TextureSpecification spec{};
    spec.Width = 2;
    spec.Height = 2;
    spec.Format = TextureFormat::RGBA8;

    auto texture = Texture2D::Create(spec);
    ASSERT_NE(texture, nullptr);

    std::array<unsigned char, 16> pixels = {
        255, 0,   0,   255,
        0,   255, 0,   255,
        0,   0,   255, 255,
        255, 255, 255, 255
    };

    EXPECT_NO_THROW(texture->SetData(pixels.data(), static_cast<uint32_t>(pixels.size())));
}

TEST_F(TextureIntegrationTests, BindAndUnbindDoNotCrash)
{
    TextureSpecification spec{};
    spec.Width = 1;
    spec.Height = 1;
    spec.Format = TextureFormat::RGBA8;

    auto texture = Texture2D::Create(spec);
    ASSERT_NE(texture, nullptr);

    EXPECT_NO_THROW(texture->Bind(0));
    EXPECT_NO_THROW(texture->Unbind());
}