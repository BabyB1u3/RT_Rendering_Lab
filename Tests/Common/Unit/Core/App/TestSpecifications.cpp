#include <gtest/gtest.h>

#include "Core/App/Application.h"
#include "Core/App/Window.h"

TEST(SpecificationTests, ApplicationSpecificationHasExpectedDefaults)
{
    ApplicationSpecification spec{};

    EXPECT_EQ(spec.Name, "RTRLab");
    EXPECT_EQ(spec.Width, 1600u);
    EXPECT_EQ(spec.Height, 900u);
}

TEST(SpecificationTests, ApplicationSpecificationCanBeCustomized)
{
    ApplicationSpecification spec{};
    spec.Name = "MyApp";
    spec.Width = 1920;
    spec.Height = 1080;

    EXPECT_EQ(spec.Name, "MyApp");
    EXPECT_EQ(spec.Width, 1920u);
    EXPECT_EQ(spec.Height, 1080u);
}

TEST(SpecificationTests, WindowPropsHasExpectedDefaults)
{
    WindowProps props{};

    EXPECT_EQ(props.Title, "RTRLab");
    EXPECT_EQ(props.Width, 1600u);
    EXPECT_EQ(props.Height, 900u);
}

TEST(SpecificationTests, WindowPropsCanBeCustomized)
{
    WindowProps props{};
    props.Title = "Main Window";
    props.Width = 1280;
    props.Height = 720;

    EXPECT_EQ(props.Title, "Main Window");
    EXPECT_EQ(props.Width, 1280u);
    EXPECT_EQ(props.Height, 720u);
}
