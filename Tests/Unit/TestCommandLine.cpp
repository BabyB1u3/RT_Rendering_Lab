#include <gtest/gtest.h>

#include "Core/Util/CommandLine.h"

namespace
{
    Util::CommandLineSpec BuildCommandLineSpec()
    {
        Util::CommandLineSpec spec;
        spec.AddFlag("help", 'h', "Show help.")
            .AddValueOption("root", std::nullopt, "path", "Override root path.")
            .AddValueOption("layout", std::nullopt, "name", "Select output layout.")
            .AddFlag("dev-mode", std::nullopt, "Enable development-only behavior.",
                     Util::CommandLineOptionVisibility::DevelopmentOnly);
        return spec;
    }
}

TEST(CommandLineTests, ParseSupportsFlagsValueOptionsAndShortAliases)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char *> argv{
        const_cast<char *>("tool"),
        const_cast<char *>("-h"),
        const_cast<char *>("--root"),
        const_cast<char *>("D:/Repo"),
        const_cast<char *>("--layout=build"),
    };

    ASSERT_TRUE(Util::ParseCommandLine(
        static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage))
        << errorMessage;
    EXPECT_TRUE(parsed.HasOption("help"));
    ASSERT_TRUE(parsed.GetOptionValue("root").has_value());
    EXPECT_EQ(*parsed.GetOptionValue("root"), "D:/Repo");
    ASSERT_TRUE(parsed.GetOptionValue("layout").has_value());
    EXPECT_EQ(*parsed.GetOptionValue("layout"), "build");
}

TEST(CommandLineTests, ParseRejectsUnknownArguments)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char *> argv{
        const_cast<char *>("tool"),
        const_cast<char *>("--mystery"),
    };

    EXPECT_FALSE(Util::ParseCommandLine(
        static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage));
    EXPECT_NE(errorMessage.find("Unknown argument"), std::string::npos);
}

TEST(CommandLineTests, ParseRejectsMissingValues)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char *> argv{
        const_cast<char *>("tool"),
        const_cast<char *>("--root"),
    };

    EXPECT_FALSE(Util::ParseCommandLine(
        static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage));
    EXPECT_NE(errorMessage.find("Missing value"), std::string::npos);
}

TEST(CommandLineTests, ProcessCommandLineStorageRoundTripsParsedState)
{
    Util::ClearProcessCommandLine();

    Util::ParsedCommandLine parsed;
    parsed.options.emplace("root", std::string("D:/Repo"));
    parsed.options.emplace("help", std::nullopt);
    parsed.positionals.emplace_back("scene.rtr");

    Util::SetProcessCommandLine(parsed);

    const auto &stored = Util::GetProcessCommandLine();
    EXPECT_TRUE(stored.HasOption("help"));
    ASSERT_TRUE(stored.GetOptionValue("root").has_value());
    EXPECT_EQ(*stored.GetOptionValue("root"), "D:/Repo");
    ASSERT_EQ(stored.positionals.size(), 1u);
    EXPECT_EQ(stored.positionals[0], "scene.rtr");

    Util::ClearProcessCommandLine();
}
