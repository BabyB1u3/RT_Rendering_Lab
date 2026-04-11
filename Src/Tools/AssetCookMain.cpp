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
            .AddValueOption("out", std::nullopt, "path", "Explicit cooked output root.")
            .AddValueOption("layout", std::nullopt, "name", "Cook output layout: cache or build.");
        return spec;
    }
}

int main(int argc, char **argv)
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

    std::filesystem::path rootPath = std::filesystem::current_path();
    std::filesystem::path cookedRootPath;
    Resource::CookOutputLayout layout = Resource::CookOutputLayout::Cache;
    bool layoutExplicitlySet = false;
    if (const auto rootOverride = commandLine.GetOptionValue("root"))
        rootPath = std::string(*rootOverride);
    if (const auto outputOverride = commandLine.GetOptionValue("out"))
        cookedRootPath = std::string(*outputOverride);
    if (const auto layoutValue = commandLine.GetOptionValue("layout"))
    {
        if (*layoutValue == "cache")
            layout = Resource::CookOutputLayout::Cache;
        else if (*layoutValue == "build")
            layout = Resource::CookOutputLayout::Build;
        else
        {
            std::cerr << "Unknown cook layout: " << *layoutValue << "\n\n"
                      << commandLineSpec.BuildUsage("rtr_asset_cook");
            return 1;
        }

        layoutExplicitlySet = true;
    }

    if (cookedRootPath.empty())
        cookedRootPath = Resource::GetCookOutputRoot(rootPath, layout);
    else if (layoutExplicitlySet)
    {
        std::cerr << "Use either --out or --layout, not both\n\n"
                  << commandLineSpec.BuildUsage("rtr_asset_cook");
        return 1;
    }

    if (!Resource::CookRepositoryCatalogs(rootPath, cookedRootPath, kProjectContentDirName, &errorMessage))
    {
        std::cerr << "rtr_asset_cook: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Cooked catalogs under " << cookedRootPath.string() << "\n";
    return 0;
}
