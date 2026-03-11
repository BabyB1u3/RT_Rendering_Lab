#include <gtest/gtest.h>
#include <memory>

#include "GlTestContext.h"

class ShaderIntegrationTests : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        s_Context = std::make_unique<GlTestContext>();
    }

    static void TearDownTestSuite()
    {
        s_Context.reset();
    }

    inline static std::unique_ptr<GlTestContext> s_Context;
};

TEST_F(ShaderIntegrationTests, OpenGLContextIsCreatedSuccessfully)
{
    ASSERT_NE(s_Context, nullptr);
    EXPECT_TRUE(s_Context->IsValid());
    EXPECT_NE(s_Context->GetWindow(), nullptr);
}