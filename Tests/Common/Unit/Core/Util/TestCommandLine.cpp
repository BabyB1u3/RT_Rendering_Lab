#include <gtest/gtest.h>

#include "Core/Util/CommandLine.h"

namespace
{
Util::CommandLineSpec BuildCommandLineSpec()
{
    Util::CommandLineSpec spec;
    spec.AddFlag("help", 'h', "Show help.")
        .AddValueOption(
            "root", std::nullopt, "path", "Override root path.", Util::CommandLineOptionVisibility::DevelopmentOnly)
        .AddValueOption("out", std::nullopt, "path", "Select output path.")
        .AddFlag("dev-mode", std::nullopt, "Enable development-only behavior.");
    return spec;
}
} // namespace

TEST(CommandLineTests, ParseSupportsFlagsValueOptionsAndShortAliases)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char*> argv{
        const_cast<char*>("tool"),
        const_cast<char*>("-h"),
        const_cast<char*>("--root"),
        const_cast<char*>("D:/Repo"),
        const_cast<char*>("--out=D:/Cooked"),
    };

    ASSERT_TRUE(Util::ParseCommandLine(static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage))
        << errorMessage;
    EXPECT_TRUE(parsed.HasOption("help"));
    ASSERT_TRUE(parsed.GetOptionValue("root").has_value());
    EXPECT_EQ(*parsed.GetOptionValue("root"), "D:/Repo");
    ASSERT_TRUE(parsed.GetOptionValue("out").has_value());
    EXPECT_EQ(*parsed.GetOptionValue("out"), "D:/Cooked");
}

TEST(CommandLineTests, ParseRejectsUnknownArguments)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char*> argv{
        const_cast<char*>("tool"),
        const_cast<char*>("--mystery"),
    };

    EXPECT_FALSE(Util::ParseCommandLine(static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage));
    EXPECT_NE(errorMessage.find("Unknown argument"), std::string::npos);
}

TEST(CommandLineTests, ParseRejectsMissingValues)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char*> argv{
        const_cast<char*>("tool"),
        const_cast<char*>("--root"),
    };

    EXPECT_FALSE(Util::ParseCommandLine(static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage));
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

    const auto& stored = Util::GetProcessCommandLine();
    EXPECT_TRUE(stored.HasOption("help"));
    ASSERT_TRUE(stored.GetOptionValue("root").has_value());
    EXPECT_EQ(*stored.GetOptionValue("root"), "D:/Repo");
    ASSERT_EQ(stored.positionals.size(), 1u);
    EXPECT_EQ(stored.positionals[0], "scene.rtr");

    Util::ClearProcessCommandLine();
}

TEST(CommandLineTests, ShippingModeDropsDevelopmentOnlyOptions)
{
    auto spec = BuildCommandLineSpec();
    Util::ParsedCommandLine parsed;
    std::string errorMessage;

    std::vector<char*> argv{
        const_cast<char*>("tool"),
        const_cast<char*>("--root"),
        const_cast<char*>("D:/Repo"),
        const_cast<char*>("--help"),
        const_cast<char*>("--dev-mode"),
    };

    ASSERT_TRUE(Util::ParseCommandLine(
        static_cast<int>(argv.size()), argv.data(), spec, parsed, &errorMessage, Util::CommandLineParseMode::Shipping))
        << errorMessage;
    EXPECT_TRUE(parsed.HasOption("help"));
    EXPECT_FALSE(parsed.HasOption("root"));
    EXPECT_TRUE(parsed.HasOption("dev-mode"));
}

TEST(CommandLineTests, ShippingUsageHidesDevelopmentOnlyOptions)
{
    const auto spec = BuildCommandLineSpec();
    const std::string usage = spec.BuildUsage("tool", Util::CommandLineParseMode::Shipping);

    EXPECT_NE(usage.find("--help"), std::string::npos);
    EXPECT_NE(usage.find("--dev-mode"), std::string::npos);
    EXPECT_EQ(usage.find("--root"), std::string::npos);
}
