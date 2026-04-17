#include <gtest/gtest.h>

#include "Core/App/Application.h"
#include "Core/App/Window/Window.h"

TEST(SpecificationTests, ApplicationSpecificationHasExpectedDefaults)
{
    ApplicationSpecification spec{};

    EXPECT_EQ(spec.m_Name, "RTRLab");
    EXPECT_EQ(spec.m_Width, 1600u);
    EXPECT_EQ(spec.m_Height, 900u);
}

TEST(SpecificationTests, ApplicationSpecificationCanBeCustomized)
{
    ApplicationSpecification spec{};
    spec.m_Name = "MyApp";
    spec.m_Width = 1920;
    spec.m_Height = 1080;

    EXPECT_EQ(spec.m_Name, "MyApp");
    EXPECT_EQ(spec.m_Width, 1920u);
    EXPECT_EQ(spec.m_Height, 1080u);
}

TEST(SpecificationTests, WindowPropsHasExpectedDefaults)
{
    WindowProps props{};

    EXPECT_EQ(props.m_Title, "RTRLab");
    EXPECT_EQ(props.m_Width, 1600u);
    EXPECT_EQ(props.m_Height, 900u);
}

TEST(SpecificationTests, WindowPropsCanBeCustomized)
{
    WindowProps props{};
    props.m_Title = "Main Window";
    props.m_Width = 1280;
    props.m_Height = 720;

    EXPECT_EQ(props.m_Title, "Main Window");
    EXPECT_EQ(props.m_Width, 1280u);
    EXPECT_EQ(props.m_Height, 720u);
}
