#include "Core/Resource/Cook/CookedCatalog.h"
#include "Core/Resource/Package/PakArchive.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    void PrintUsage()
    {
        std::cout << "Usage: rtr_asset_pack [--root <path>] [--source <path>] [--out <path>] [--layout <cache|build>]\n";
    }
}

int main(int argc, char **argv)
{
    std::filesystem::path rootPath = std::filesystem::current_path();
    std::filesystem::path cookedRootPath;
    std::filesystem::path packagedRootPath;
    Resource::CookOutputLayout layout = Resource::CookOutputLayout::Cache;
    bool layoutExplicitlySet = false;

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

        if (arg == "--source")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for --source\n";
                PrintUsage();
                return 1;
            }

            cookedRootPath = argv[++i];
            continue;
        }

        if (arg == "--out")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for --out\n";
                PrintUsage();
                return 1;
            }

            packagedRootPath = argv[++i];
            continue;
        }

        if (arg == "--layout")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for --layout\n";
                PrintUsage();
                return 1;
            }

            const std::string_view value = argv[++i];
            if (value == "cache")
                layout = Resource::CookOutputLayout::Cache;
            else if (value == "build")
                layout = Resource::CookOutputLayout::Build;
            else
            {
                std::cerr << "Unknown package layout: " << value << "\n";
                PrintUsage();
                return 1;
            }

            layoutExplicitlySet = true;
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        PrintUsage();
        return 1;
    }

    if (cookedRootPath.empty())
        cookedRootPath = Resource::GetCookOutputRoot(rootPath, layout);

    if (packagedRootPath.empty())
        packagedRootPath = Resource::GetPackagedOutputRoot(rootPath, layout);
    else if (layoutExplicitlySet)
    {
        std::cerr << "Use either --out or --layout, not both\n";
        PrintUsage();
        return 1;
    }

    std::string errorMessage;
    if (!Resource::PackageCookedRepositoryCatalogs(cookedRootPath, packagedRootPath, &errorMessage))
    {
        std::cerr << "rtr_asset_pack: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Packaged archive: " << Resource::GetGamePackagedArchivePath(packagedRootPath).string() << "\n";
    return 0;
}
