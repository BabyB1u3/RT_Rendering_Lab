#include "Core/Resource/CookedCatalog.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    constexpr std::string_view kProjectContentDirName = "Content";

    void PrintUsage()
    {
        std::cout << "Usage: rtr_asset_cook [--root <path>] [--out <path>]\n";
    }
}

int main(int argc, char **argv)
{
    std::filesystem::path rootPath = std::filesystem::current_path();
    std::filesystem::path cookedRootPath;

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

        if (arg == "--out")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for --out\n";
                PrintUsage();
                return 1;
            }

            cookedRootPath = argv[++i];
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        PrintUsage();
        return 1;
    }

    if (cookedRootPath.empty())
        cookedRootPath = rootPath / "Saved" / "Cache" / "Cooked";

    std::string errorMessage;
    if (!Resource::CookRepositoryCatalogs(rootPath, cookedRootPath, kProjectContentDirName, &errorMessage))
    {
        std::cerr << "rtr_asset_cook: " << errorMessage << "\n";
        return 1;
    }

    std::cout << "Cooked catalogs under " << cookedRootPath.string() << "\n";
    return 0;
}
