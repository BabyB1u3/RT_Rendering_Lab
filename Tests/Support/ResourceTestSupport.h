#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "TestPaths.h"

namespace test_support
{
    inline void WriteProjectMarkerOrFail(const std::filesystem::path &rootPath)
    {
        WriteTextFileOrFail(rootPath / ".rtrproject", "\n");
    }

    inline std::filesystem::path CreateRepoRootOrFail(const std::filesystem::path &testRoot)
    {
        const auto repoRoot = testRoot / "Repo";
        WriteProjectMarkerOrFail(repoRoot);
        return repoRoot;
    }

    inline std::filesystem::path ProjectContentRoot(const std::filesystem::path &repoRoot)
    {
        return repoRoot / "Content";
    }

    inline std::filesystem::path EngineContentRoot(const std::filesystem::path &repoRoot)
    {
        return repoRoot / "EngineContent";
    }

    inline std::filesystem::path PluginContentRoot(const std::filesystem::path &repoRoot, std::string_view pluginName)
    {
        return repoRoot / "Plugins" / std::filesystem::path(std::string(pluginName)) / "Content";
    }

    inline void WriteProjectFileOrFail(
        const std::filesystem::path &repoRoot,
        const std::filesystem::path &relativePath,
        std::string_view contents)
    {
        WriteTextFileOrFail(ProjectContentRoot(repoRoot) / relativePath, contents);
    }

    inline void WriteProjectBinaryFileOrFail(
        const std::filesystem::path &repoRoot,
        const std::filesystem::path &relativePath,
        const std::vector<unsigned char> &contents)
    {
        WriteBinaryFileOrFail(ProjectContentRoot(repoRoot) / relativePath, contents);
    }

    inline void WriteEngineFileOrFail(
        const std::filesystem::path &repoRoot,
        const std::filesystem::path &relativePath,
        std::string_view contents)
    {
        WriteTextFileOrFail(EngineContentRoot(repoRoot) / relativePath, contents);
    }

    inline void WritePluginFileOrFail(
        const std::filesystem::path &repoRoot,
        std::string_view pluginName,
        const std::filesystem::path &relativePath,
        std::string_view contents)
    {
        WriteTextFileOrFail(PluginContentRoot(repoRoot, pluginName) / relativePath, contents);
    }

    inline std::filesystem::path ProjectSourceCatalogPath(const std::filesystem::path &repoRoot)
    {
        return ProjectContentRoot(repoRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path EngineSourceCatalogPath(const std::filesystem::path &repoRoot)
    {
        return EngineContentRoot(repoRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path PluginSourceCatalogPath(
        const std::filesystem::path &repoRoot,
        std::string_view pluginName)
    {
        return PluginContentRoot(repoRoot, pluginName) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path CookedRoot(const std::filesystem::path &repoRoot)
    {
        return repoRoot / "Saved" / "Cache" / "Cooked";
    }

    inline std::filesystem::path ProjectCookedRoot(const std::filesystem::path &cookedRoot)
    {
        return cookedRoot / "Project";
    }

    inline std::filesystem::path EngineCookedRoot(const std::filesystem::path &cookedRoot)
    {
        return cookedRoot / "Engine";
    }

    inline std::filesystem::path PluginCookedRoot(const std::filesystem::path &cookedRoot, std::string_view pluginName)
    {
        return cookedRoot / "Plugins" / std::filesystem::path(std::string(pluginName));
    }

    inline std::filesystem::path ProjectCookedCatalogPath(const std::filesystem::path &cookedRoot)
    {
        return ProjectCookedRoot(cookedRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path EngineCookedCatalogPath(const std::filesystem::path &cookedRoot)
    {
        return EngineCookedRoot(cookedRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path PluginCookedCatalogPath(
        const std::filesystem::path &cookedRoot,
        std::string_view pluginName)
    {
        return PluginCookedRoot(cookedRoot, pluginName) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path ProjectPackagedArchivePath(const std::filesystem::path &packagedRoot)
    {
        return packagedRoot / "Project.rtrpak";
    }

    inline std::filesystem::path EnginePackagedArchivePath(const std::filesystem::path &packagedRoot)
    {
        return packagedRoot / "Engine.rtrpak";
    }

    inline std::filesystem::path PluginPackagedArchivePath(
        const std::filesystem::path &packagedRoot,
        std::string_view pluginName)
    {
        return packagedRoot / "Plugins" / std::filesystem::path(std::string(pluginName) + ".rtrpak");
    }
} // namespace test_support
