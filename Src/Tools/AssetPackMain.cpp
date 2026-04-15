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
        .AddValueOption("source", std::nullopt, "path", "Cooked source directory to package.")
        .AddValueOption("out", std::nullopt, "path", "Packaged output root.");
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
        std::cerr << errorMessage << "\n\n" << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 1;
    }

    if (commandLine.HasOption("help"))
    {
        std::cout << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 0;
    }

    const auto sourceOverride = commandLine.GetOptionValue("source");
    if (!sourceOverride.has_value() || sourceOverride->empty())
    {
        std::cerr << "Missing required argument: --source\n\n" << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 1;
    }

    const auto outputOverride = commandLine.GetOptionValue("out");
    if (!outputOverride.has_value() || outputOverride->empty())
    {
        std::cerr << "Missing required argument: --out\n\n" << commandLineSpec.BuildUsage("rtr_asset_pack");
        return 1;
    }

    const std::filesystem::path cookedRootPath = std::string(*sourceOverride);
    const std::filesystem::path packagedRootPath = std::string(*outputOverride);

    if (!Resource::PackageCookedRepositoryCatalogs(cookedRootPath, packagedRootPath, &errorMessage))
    {
        std::cerr << "rtr_asset_pack: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Packaged archive: " << Resource::GetGamePackagedArchivePath(packagedRootPath).string() << "\n";
    return 0;
}
