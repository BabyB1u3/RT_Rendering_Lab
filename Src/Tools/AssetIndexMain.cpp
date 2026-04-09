#include "Core/Resource/Catalog/SourceCatalog.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    constexpr std::string_view kProjectContentDirName = "Content";

    void PrintUsage()
    {
        std::cout << "Usage: rtr_asset_index [--root <path>]\n";
    }
}

int main(int argc, char **argv)
{
    std::filesystem::path rootPath = std::filesystem::current_path();

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            PrintUsage();
            return 0;
        }

        if (arg == "--root")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for --root\n";
                PrintUsage();
                return 1;
            }

            rootPath = argv[++i];
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        PrintUsage();
        return 1;
    }

    std::string errorMessage;
    if (!Resource::IndexRepositorySourceCatalogs(rootPath, kProjectContentDirName, &errorMessage))
    {
        std::cerr << "rtr_asset_index: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Indexed source catalogs under " << rootPath.string() << "\n";
    return 0;
}
