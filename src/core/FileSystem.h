#pragma once

#include <filesystem>
#include <string>
#include <string_view>

class FileSystem
{
public:
    static void Init();

    static const std::filesystem::path &GetRootPath();
    static std::filesystem::path GetAssetPath(std::string_view relativePath);
    static std::filesystem::path GetShaderPath(std::string_view shaderName);

    static std::string ReadTextFile(const std::filesystem::path &path);
    static bool Exists(const std::filesystem::path &path);

private:
    static std::filesystem::path s_RootPath;
    static bool s_Initialized;

    static std::filesystem::path DiscoverRootPath();
    static std::filesystem::path FindRootFromExecutable();
};
