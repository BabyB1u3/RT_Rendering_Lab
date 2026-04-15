#include "Core/Resource/Cook/CookedCatalog.h"
#include "Core/Util/CommandLine.h"

#include <filesystem>
#include <iostream>

namespace
{
constexpr std::string_view kProjectContentDirName = "Project";

Util::CommandLineSpec BuildCommandLineSpec()
{
    Util::CommandLineSpec spec;
    spec.AddFlag("help", 'h', "Show command-line help and exit.")
        .AddValueOption("root", std::nullopt, "path", "Repository root to cook.")
        .AddValueOption("out", std::nullopt, "path", "Cooked output root.");
    return spec;
}
} // namespace

int main(int argc, char** argv)
{
    const auto commandLineSpec = BuildCommandLineSpec();
    Util::ParsedCommandLine commandLine;
    std::string errorMessage;
    if (!Util::ParseCommandLine(argc, argv, commandLineSpec, commandLine, &errorMessage))
    {
        std::cerr << errorMessage << "\n\n" << commandLineSpec.BuildUsage("rtr_asset_cook");
        return 1;
    }

    if (commandLine.HasOption("help"))
    {
        std::cout << commandLineSpec.BuildUsage("rtr_asset_cook");
        return 0;
    }

    const auto rootOverride = commandLine.GetOptionValue("root");
    if (!rootOverride.has_value() || rootOverride->empty())
    {
        std::cerr << "Missing required argument: --root\n\n" << commandLineSpec.BuildUsage("rtr_asset_cook");
        return 1;
    }

    const auto outputOverride = commandLine.GetOptionValue("out");
    if (!outputOverride.has_value() || outputOverride->empty())
    {
        std::cerr << "Missing required argument: --out\n\n" << commandLineSpec.BuildUsage("rtr_asset_cook");
        return 1;
    }

    const std::filesystem::path rootPath = std::string(*rootOverride);
    const std::filesystem::path cookedRootPath = std::string(*outputOverride);

    if (!Resource::CookRepositoryCatalogs(rootPath, cookedRootPath, kProjectContentDirName, &errorMessage))
    {
        std::cerr << "rtr_asset_cook: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Cooked catalogs under " << cookedRootPath.string() << "\n";
    return 0;
}
