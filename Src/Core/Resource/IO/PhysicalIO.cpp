#include "Core/Resource/IO/PhysicalIO.h"

#include <fstream>
#include <sstream>

#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

namespace Resource
{
std::optional<std::string> ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed to open text file: {}", path.string());
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in.good() && !in.eof())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed while reading text file: {}", path.string());
        return std::nullopt;
    }

    return ss.str();
}

std::optional<std::vector<uint8_t>> ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!in)
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed to open binary file: {}", path.string());
        return std::nullopt;
    }

    const auto size = in.tellg();
    if (size < 0)
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed to query binary file size: {}", path.string());
        return std::nullopt;
    }

    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!data.empty())
        in.read(reinterpret_cast<char*>(data.data()), size);

    if (in.fail())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed while reading binary file: {}", path.string());
        return std::nullopt;
    }

    return data;
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view data)
{
    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed to open text file for writing: {}", path.string());
        return false;
    }

    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out.good())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed while writing text file: {}", path.string());
        return false;
    }

    return true;
}

bool WriteBinaryFile(const std::filesystem::path& path, std::span<const uint8_t> data)
{
    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed to open binary file for writing: {}", path.string());
        return false;
    }

    if (!data.empty())
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    if (!out.good())
    {
        LOG_ERROR_CAT(LogCategory::k_FileSystem, "Failed while writing binary file: {}", path.string());
        return false;
    }

    return true;
}
} // namespace Resource
