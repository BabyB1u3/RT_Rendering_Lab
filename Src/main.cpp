/// @file main.cpp
/// @brief Application entry point.
///
/// Creates the Application with a window spec, pushes LabLayer as the sole
/// Layer, and enters the main loop. LabLayer handles demo registration,
/// switching, and all per-frame callbacks.

#include "Core/App/Application.h"
#include "Core/Util/Base.h"
#include "Core/Util/CommandLine.h"
#include "Demos/LabLayer.h"

#include <iostream>

namespace
{
    Util::CommandLineSpec BuildMainCommandLineSpec()
    {
        Util::CommandLineSpec spec;
        spec.AddFlag("help", 'h', "Show command-line help and exit.")
            .AddValueOption("language", std::nullopt, "locale", "Requested language override.")
            .AddFlag("windowed", std::nullopt, "Request windowed mode.")
            .AddFlag("fullscreen", std::nullopt, "Request fullscreen mode.")
            .AddFlag("dev-mode", std::nullopt, "Enable development-only command-line overrides in shipping builds.")
            .AddValueOption("root", std::nullopt, "path", "Development-only root override.",
                            Util::CommandLineOptionVisibility::DevelopmentOnly)
            .AddValueOption("cooked-root", std::nullopt, "path", "Development-only cooked root override.",
                            Util::CommandLineOptionVisibility::DevelopmentOnly)
            .AddValueOption("package-root", std::nullopt, "path", "Development-only packaged root override.",
                            Util::CommandLineOptionVisibility::DevelopmentOnly)
            .AddValueOption("resource-profile", std::nullopt, "name", "Development-only resource profile override.",
                            Util::CommandLineOptionVisibility::DevelopmentOnly);
        return spec;
    }
}

int main(int argc, char **argv)
{
    const auto commandLineSpec = BuildMainCommandLineSpec();
    Util::ParsedCommandLine commandLine;
    std::string errorMessage;
    if (!Util::ParseCommandLine(argc, argv, commandLineSpec, commandLine, &errorMessage))
    {
        std::cerr << errorMessage << "\n\n" << commandLineSpec.BuildUsage("RTRLab");
        return 1;
    }

    if (commandLine.HasOption("help"))
    {
        std::cout << commandLineSpec.BuildUsage("RTRLab");
        return 0;
    }

    Util::SetProcessCommandLine(std::move(commandLine));

    ApplicationSpecification spec;
    spec.Name = "RTRLab";
    spec.Width = 1600;
    spec.Height = 900;
    spec.VSync = true;

    Application app(spec);
    app.PushLayer(CreateScope<LabLayer>());
    app.Run();

    return 0;
}
