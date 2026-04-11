#include "Core/Resource/Cook/CookedCatalog.h"
#include "Core/Resource/Package/PakArchive.h"
#include "Core/Util/CommandLine.h"

#include <filesystem>
#include <iostream>

namespace
{
    Util::CommandLineSpec BuildCommandLineSpec()
    {
        Util::CommandLineSpec spec;
        spec.AddFlag("help", 'h', "Show command-line help and exit.")
            .AddValueOption("root", std::nullopt, "path", "Repository root used for default output layout.")
            .AddValueOption("source", std::nullopt, "path", "Cooked source directory to package.")
            .AddValueOption("out", std::nullopt, "path", "Explicit packaged output root.")
            .AddValueOption("layout", std::nullopt, "name", "Packaged output layout: cache or build.");
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
        std::cerr << errorMessage << "\n\n" << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 1;
    }

    if (commandLine.HasOption("help"))
    {
        std::cout << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 0;
    }

    std::filesystem::path rootPath = std::filesystem::current_path();
    std::filesystem::path cookedRootPath;
    std::filesystem::path packagedRootPath;
    Resource::CookOutputLayout layout = Resource::CookOutputLayout::Cache;
    bool layoutExplicitlySet = false;
    if (const auto rootOverride = commandLine.GetOptionValue("root"))
        rootPath = std::string(*rootOverride);
    if (const auto sourceOverride = commandLine.GetOptionValue("source"))
        cookedRootPath = std::string(*sourceOverride);
    if (const auto outputOverride = commandLine.GetOptionValue("out"))
        packagedRootPath = std::string(*outputOverride);
    if (const auto layoutValue = commandLine.GetOptionValue("layout"))
    {
        if (*layoutValue == "cache")
            layout = Resource::CookOutputLayout::Cache;
        else if (*layoutValue == "build")
            layout = Resource::CookOutputLayout::Build;
        else
        {
            std::cerr << "Unknown package layout: " << *layoutValue << "\n\n"
                      << commandLineSpec.BuildUsage("rtr_asset_pack");
            return 1;
        }

        layoutExplicitlySet = true;
    }

    if (cookedRootPath.empty())
        cookedRootPath = Resource::GetCookOutputRoot(rootPath, layout);

    if (packagedRootPath.empty())
        packagedRootPath = Resource::GetPackagedOutputRoot(rootPath, layout);
    else if (layoutExplicitlySet)
    {
        std::cerr << "Use either --out or --layout, not both\n\n"
                  << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 1;
    }

    if (!Resource::PackageCookedRepositoryCatalogs(cookedRootPath, packagedRootPath, &errorMessage))
    {
        std::cerr << "rtr_asset_pack: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Packaged archive: " << Resource::GetGamePackagedArchivePath(packagedRootPath).string() << "\n";
    return 0;
}
