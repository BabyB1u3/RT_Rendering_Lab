#include "Core/Resource/Catalog/SourceCatalog.h"
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
            .AddValueOption("root", std::nullopt, "path", "Repository root to index.");
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
        std::cerr << errorMessage << "\n\n" << commandLineSpec.BuildUsage("rtr_asset_index");
        return 1;
    }

    if (commandLine.HasOption("help"))
    {
        std::cout << commandLineSpec.BuildUsage("rtr_asset_index");
        return 0;
    }

    std::filesystem::path rootPath = std::filesystem::current_path();
    if (const auto rootOverride = commandLine.GetOptionValue("root"))
        rootPath = std::string(*rootOverride);

    if (!Resource::IndexRepositorySourceCatalogs(rootPath, kProjectContentDirName, &errorMessage))
    {
        std::cerr << "rtr_asset_index: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Indexed source catalogs under " << rootPath.string() << "\n";
    return 0;
}
