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
        return repoRoot / "Project";
    }

    inline std::filesystem::path EngineRoot(const std::filesystem::path &repoRoot)
    {
        return repoRoot / "Engine";
    }

    inline std::filesystem::path MountCatalogPath(const std::filesystem::path &mountRoot)
    {
        return mountRoot / ".rtr" / "catalog.json";
    }

    inline void WriteMountFileOrFail(
        const std::filesystem::path &mountRoot,
        const std::filesystem::path &relativePath,
        std::string_view contents)
    {
        WriteTextFileOrFail(mountRoot / relativePath, contents);
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
        WriteTextFileOrFail(EngineRoot(repoRoot) / relativePath, contents);
    }

    inline std::filesystem::path ProjectSourceCatalogPath(const std::filesystem::path &repoRoot)
    {
        return ProjectContentRoot(repoRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path EngineSourceCatalogPath(const std::filesystem::path &repoRoot)
    {
        return EngineRoot(repoRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path CookedRoot(const std::filesystem::path &repoRoot)
    {
        return repoRoot / "Saved" / "Cache" / "Cooked";
    }

    inline std::filesystem::path BuildCookedRoot(const std::filesystem::path &repoRoot)
    {
        return repoRoot / "build" / "Cooked";
    }

    inline std::filesystem::path ProjectCookedRoot(const std::filesystem::path &cookedRoot)
    {
        return cookedRoot / "Project";
    }

    inline std::filesystem::path EngineCookedRoot(const std::filesystem::path &cookedRoot)
    {
        return cookedRoot / "Engine";
    }

    inline std::filesystem::path ProjectCookedCatalogPath(const std::filesystem::path &cookedRoot)
    {
        return ProjectCookedRoot(cookedRoot) / ".rtr" / "catalog.json";
    }

    inline std::filesystem::path EngineCookedCatalogPath(const std::filesystem::path &cookedRoot)
    {
        return EngineCookedRoot(cookedRoot) / ".rtr" / "catalog.json";
    }

    inline void WriteProjectCookedFileOrFail(
        const std::filesystem::path &cookedRoot,
        const std::filesystem::path &relativePath,
        std::string_view contents)
    {
        WriteTextFileOrFail(ProjectCookedRoot(cookedRoot) / relativePath, contents);
    }

    inline void WriteEngineCookedFileOrFail(
        const std::filesystem::path &cookedRoot,
        const std::filesystem::path &relativePath,
        std::string_view contents)
    {
        WriteTextFileOrFail(EngineCookedRoot(cookedRoot) / relativePath, contents);
    }

    inline std::filesystem::path ProjectMaterializedRoot(const std::filesystem::path &extractedRoot)
    {
        return extractedRoot / "Project";
    }

    inline std::filesystem::path GamePackagedArchivePath(const std::filesystem::path &packagedRoot)
    {
        return packagedRoot / "Game.rtrpak";
    }

} // namespace test_support
