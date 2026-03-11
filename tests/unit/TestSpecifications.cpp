#include <gtest/gtest.h>
#include <glad/glad.h>

#include "core/Application.h"
#include "core/Window.h"
#include "graphics/Texture.h"
#include "graphics/Framebuffer.h"

TEST(SpecificationTests, ApplicationSpecificationHasExpectedDefaults)
{
    ApplicationSpecification spec{};

    EXPECT_EQ(spec.Name, "RTRLab");
    EXPECT_EQ(spec.Width, 1600u);
    EXPECT_EQ(spec.Height, 900u);
    EXPECT_TRUE(spec.VSync);
}

TEST(SpecificationTests, ApplicationSpecificationCanBeCustomized)
{
    ApplicationSpecification spec{};
    spec.Name = "MyApp";
    spec.Width = 1920;
    spec.Height = 1080;
    spec.VSync = false;

    EXPECT_EQ(spec.Name, "MyApp");
    EXPECT_EQ(spec.Width, 1920u);
    EXPECT_EQ(spec.Height, 1080u);
    EXPECT_FALSE(spec.VSync);
}

TEST(SpecificationTests, WindowPropsHasExpectedDefaults)
{
    WindowProps props{};

    EXPECT_EQ(props.Title, "RTRLab");
    EXPECT_EQ(props.Width, 1600u);
    EXPECT_EQ(props.Height, 900u);
    EXPECT_TRUE(props.VSync);
}

TEST(SpecificationTests, WindowPropsCanBeCustomized)
{
    WindowProps props{};
    props.Title = "Main Window";
    props.Width = 1280;
    props.Height = 720;
    props.VSync = false;

    EXPECT_EQ(props.Title, "Main Window");
    EXPECT_EQ(props.Width, 1280u);
    EXPECT_EQ(props.Height, 720u);
    EXPECT_FALSE(props.VSync);
}

TEST(SpecificationTests, TextureSpecificationHasExpectedDefaults)
{
    TextureSpecification spec{};

    EXPECT_EQ(spec.Width, 1u);
    EXPECT_EQ(spec.Height, 1u);
    EXPECT_EQ(spec.Format, TextureFormat::RGBA8);
    EXPECT_FALSE(spec.GenerateMips);

    EXPECT_EQ(spec.WrapS, static_cast<GLenum>(GL_REPEAT));
    EXPECT_EQ(spec.WrapT, static_cast<GLenum>(GL_REPEAT));
    EXPECT_EQ(spec.MinFilter, static_cast<GLenum>(GL_LINEAR));
    EXPECT_EQ(spec.MagFilter, static_cast<GLenum>(GL_LINEAR));
}

TEST(SpecificationTests, TextureSpecificationCanBeCustomized)
{
    TextureSpecification spec{};
    spec.Width = 512;
    spec.Height = 256;
    spec.Format = TextureFormat::Depth;
    spec.GenerateMips = true;
    spec.WrapS = GL_CLAMP_TO_EDGE;
    spec.WrapT = GL_CLAMP_TO_EDGE;
    spec.MinFilter = GL_NEAREST;
    spec.MagFilter = GL_NEAREST;

    EXPECT_EQ(spec.Width, 512u);
    EXPECT_EQ(spec.Height, 256u);
    EXPECT_EQ(spec.Format, TextureFormat::Depth);
    EXPECT_TRUE(spec.GenerateMips);
    EXPECT_EQ(spec.WrapS, static_cast<GLenum>(GL_CLAMP_TO_EDGE));
    EXPECT_EQ(spec.WrapT, static_cast<GLenum>(GL_CLAMP_TO_EDGE));
    EXPECT_EQ(spec.MinFilter, static_cast<GLenum>(GL_NEAREST));
    EXPECT_EQ(spec.MagFilter, static_cast<GLenum>(GL_NEAREST));
}

TEST(SpecificationTests, FramebufferTextureSpecificationDefaultsToNoneFormat)
{
    FramebufferTextureSpecification spec{};

    EXPECT_EQ(spec.Format, TextureFormat::None);
}

TEST(SpecificationTests, FramebufferTextureSpecificationSingleArgumentConstructorStoresFormat)
{
    FramebufferTextureSpecification spec(TextureFormat::Depth24Stencil8);

    EXPECT_EQ(spec.Format, TextureFormat::Depth24Stencil8);
}

TEST(SpecificationTests, FramebufferAttachmentSpecificationDefaultConstructsEmpty)
{
    FramebufferAttachmentSpecification spec{};

    EXPECT_TRUE(spec.Attachments.empty());
}

TEST(SpecificationTests, FramebufferAttachmentSpecificationInitializerListConstructorStoresAttachments)
{
    FramebufferAttachmentSpecification spec = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };

    ASSERT_EQ(spec.Attachments.size(), 2u);
    EXPECT_EQ(spec.Attachments[0].Format, TextureFormat::RGBA8);
    EXPECT_EQ(spec.Attachments[1].Format, TextureFormat::Depth24Stencil8);
}

TEST(SpecificationTests, FramebufferSpecificationHasExpectedDefaults)
{
    FramebufferSpecification spec{};

    EXPECT_EQ(spec.Width, 0u);
    EXPECT_EQ(spec.Height, 0u);
    EXPECT_TRUE(spec.Attachments.Attachments.empty());
    EXPECT_EQ(spec.Samples, 1u);
    EXPECT_FALSE(spec.SwapChainTarget);
}

TEST(SpecificationTests, FramebufferSpecificationCanBeCustomized)
{
    FramebufferSpecification spec{};
    spec.Width = 1024;
    spec.Height = 768;
    spec.Attachments = {
        TextureFormat::RGBA8,
        TextureFormat::Depth24Stencil8
    };
    spec.Samples = 4;
    spec.SwapChainTarget = true;

    EXPECT_EQ(spec.Width, 1024u);
    EXPECT_EQ(spec.Height, 768u);
    ASSERT_EQ(spec.Attachments.Attachments.size(), 2u);
    EXPECT_EQ(spec.Attachments.Attachments[0].Format, TextureFormat::RGBA8);
    EXPECT_EQ(spec.Attachments.Attachments[1].Format, TextureFormat::Depth24Stencil8);
    EXPECT_EQ(spec.Samples, 4u);
    EXPECT_TRUE(spec.SwapChainTarget);
}