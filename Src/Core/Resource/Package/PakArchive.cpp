#include "Core/Resource/Package/PakArchive.h"

#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <json.hpp>
#include <unordered_map>

namespace
{
    using Json = nlohmann::json;

    struct PakHeader
    {
        char magic[8] = {'R', 'T', 'R', 'P', 'A', 'K', '0', '1'};
        uint32_t version = 1;
        uint32_t entryCount = 0;
        uint64_t indexOffset = 0;
        uint64_t indexSize = 0;
    };

    struct PakIndexEntry
    {
        std::string relativePath;
        uint64_t dataOffset = 0;
        uint64_t dataSize = 0;
    };

    static_assert(sizeof(PakHeader) == 32);

    constexpr std::array<char, 8> kPakMagic{'R', 'T', 'R', 'P', 'A', 'K', '0', '1'};
    constexpr uint32_t kPakVersion = 1;

    bool HasExpectedMagic(const PakHeader &header)
    {
        return std::equal(kPakMagic.begin(), kPakMagic.end(), header.magic);
    }

    bool IsSafeRelativePath(const std::filesystem::path &path)
    {
        if (path.is_absolute())
            return false;

        for (const auto &segment : path)
        {
            const auto text = segment.generic_string();
            if (text == "." || text == "..")
                return false;
        }

        return true;
    }

    struct StagedPakEntry
    {
        std::filesystem::path sourcePath;
        std::string archiveRelativePath;
    };

    bool CollectPakEntriesFromRoot(const std::filesystem::path &sourceRoot,
                                   const std::filesystem::path &archiveRoot,
                                   std::vector<StagedPakEntry> &entries,
                                   std::string *errorMessage)
    {
        if (!std::filesystem::exists(sourceRoot))
            return true;

        std::error_code ec;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(sourceRoot, ec))
        {
            if (ec)
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to scan pak source root: " + ec.message();
                return false;
            }

            if (!entry.is_regular_file())
                continue;

            const auto relativePath = std::filesystem::relative(entry.path(), sourceRoot, ec);
            if (ec || !IsSafeRelativePath(relativePath))
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to derive safe relative path for pak entry: " + entry.path().string();
                return false;
            }

            entries.push_back(StagedPakEntry{
                .sourcePath = entry.path(),
                .archiveRelativePath = (archiveRoot / relativePath).generic_string(),
            });
        }

        return true;
    }

    bool LoadCookedCatalogEntries(const std::filesystem::path &catalogPath,
                                  Json &entriesJson,
                                  std::string *errorMessage)
    {
        std::ifstream in(catalogPath, std::ios::binary);
        if (!in.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open cooked catalog: " + catalogPath.string();
            return false;
        }

        Json rootJson;
        try
        {
            in >> rootJson;
        }
        catch (const std::exception &e)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to parse cooked catalog '" + catalogPath.string() + "': " + e.what();
            return false;
        }

        const auto versionIt = rootJson.find("version");
        const auto kindIt = rootJson.find("kind");
        const auto entriesIt = rootJson.find("entries");
        if (versionIt == rootJson.end() || !versionIt->is_number_integer() || versionIt->get<int>() != 2 ||
            kindIt == rootJson.end() || !kindIt->is_string() || kindIt->get<std::string>() != "cooked" ||
            entriesIt == rootJson.end() || !entriesIt->is_array())
        {
            if (errorMessage != nullptr)
                *errorMessage = "invalid cooked catalog: " + catalogPath.string();
            return false;
        }

        entriesJson = *entriesIt;
        return true;
    }

    bool BuildMergedCookedCatalogJson(const std::filesystem::path &cookedRootPath,
                                      std::string &catalogJson,
                                      std::string *errorMessage)
    {
        Json mergedEntries = Json::array();

        for (const auto &mountName : {std::string("Project"), std::string("Engine")})
        {
            const auto catalogPath = cookedRootPath / mountName / ".rtr" / "catalog.json";
            if (!std::filesystem::exists(catalogPath))
                continue;

            Json mountEntries = Json::array();
            if (!LoadCookedCatalogEntries(catalogPath, mountEntries, errorMessage))
                return false;

            for (auto &entry : mountEntries)
                mergedEntries.push_back(std::move(entry));
        }

        Json mergedCatalog{
            {"version", 2},
            {"kind", "cooked"},
            {"entries", std::move(mergedEntries)},
        };
        catalogJson = mergedCatalog.dump(2);
        return true;
    }

    bool BuildMergedGamePakArchive(const std::filesystem::path &projectRoot,
                                   const std::filesystem::path &engineRoot,
                                   const std::filesystem::path &pakPath,
                                   std::string *errorMessage)
    {
        std::vector<StagedPakEntry> stagedEntries;
        stagedEntries.reserve(64);

        if (!CollectPakEntriesFromRoot(projectRoot, "Project", stagedEntries, errorMessage))
            return false;
        if (!CollectPakEntriesFromRoot(engineRoot, "Engine", stagedEntries, errorMessage))
            return false;

        std::sort(stagedEntries.begin(), stagedEntries.end(), [](const auto &lhs, const auto &rhs)
                  { return lhs.archiveRelativePath < rhs.archiveRelativePath; });

        std::filesystem::create_directories(pakPath.parent_path());
        std::ofstream out(pakPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open pak archive for writing: " + pakPath.string();
            return false;
        }

        PakHeader header{};
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));

        std::vector<PakIndexEntry> indexEntries;
        indexEntries.reserve(stagedEntries.size() + 1);

        std::string mergedCatalogJson;
        if (!BuildMergedCookedCatalogJson(projectRoot.parent_path(), mergedCatalogJson, errorMessage))
            return false;

        const auto catalogOffset = static_cast<uint64_t>(out.tellp());
        out.write(mergedCatalogJson.data(), static_cast<std::streamsize>(mergedCatalogJson.size()));
        if (!out.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to write merged catalog into pak: " + pakPath.string();
            return false;
        }
        indexEntries.push_back(PakIndexEntry{
            .relativePath = ".rtr/catalog.json",
            .dataOffset = catalogOffset,
            .dataSize = static_cast<uint64_t>(mergedCatalogJson.size()),
        });

        for (const auto &entry : stagedEntries)
        {
            std::ifstream in(entry.sourcePath, std::ios::binary);
            if (!in.is_open())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to open pak source file: " + entry.sourcePath.string();
                return false;
            }

            const auto dataOffset = static_cast<uint64_t>(out.tellp());
            std::vector<char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            if (!out.good())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to write pak entry: " + entry.archiveRelativePath;
                return false;
            }

            indexEntries.push_back(PakIndexEntry{
                .relativePath = entry.archiveRelativePath,
                .dataOffset = dataOffset,
                .dataSize = static_cast<uint64_t>(buffer.size()),
            });
        }

        const auto indexOffset = static_cast<uint64_t>(out.tellp());
        for (const auto &entry : indexEntries)
        {
            const uint32_t pathLength = static_cast<uint32_t>(entry.relativePath.size());
            out.write(reinterpret_cast<const char *>(&pathLength), sizeof(pathLength));
            out.write(reinterpret_cast<const char *>(&entry.dataOffset), sizeof(entry.dataOffset));
            out.write(reinterpret_cast<const char *>(&entry.dataSize), sizeof(entry.dataSize));
            out.write(entry.relativePath.data(), static_cast<std::streamsize>(entry.relativePath.size()));
        }
        if (!out.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to write pak index: " + pakPath.string();
            return false;
        }

        const auto endOffset = static_cast<uint64_t>(out.tellp());
        header.entryCount = static_cast<uint32_t>(indexEntries.size());
        header.indexOffset = indexOffset;
        header.indexSize = endOffset - indexOffset;

        out.seekp(0, std::ios::beg);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        if (!out.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to finalize pak header: " + pakPath.string();
            return false;
        }

        return true;
    }

    std::string PakPathToGenericString(const std::filesystem::path &path)
    {
        return path.generic_string();
    }

    bool LoadPakIndex(const std::filesystem::path &pakPath,
                      std::vector<PakIndexEntry> &entries,
                      std::string *errorMessage)
    {
        std::ifstream in(pakPath, std::ios::binary);
        if (!in.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open pak archive: " + pakPath.string();
            return false;
        }

        PakHeader header{};
        in.read(reinterpret_cast<char *>(&header), sizeof(header));
        if (in.gcount() != static_cast<std::streamsize>(sizeof(header)))
        {
            if (errorMessage != nullptr)
                *errorMessage = "pak archive is truncated: " + pakPath.string();
            return false;
        }

        if (!HasExpectedMagic(header) || header.version != kPakVersion)
        {
            if (errorMessage != nullptr)
                *errorMessage = "pak archive has invalid header: " + pakPath.string();
            return false;
        }

        in.seekg(static_cast<std::streamoff>(header.indexOffset), std::ios::beg);
        if (!in.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "pak archive index offset is invalid: " + pakPath.string();
            return false;
        }

        entries.clear();
        entries.reserve(header.entryCount);

        for (uint32_t i = 0; i < header.entryCount; ++i)
        {
            uint32_t pathLength = 0;
            uint64_t dataOffset = 0;
            uint64_t dataSize = 0;

            in.read(reinterpret_cast<char *>(&pathLength), sizeof(pathLength));
            in.read(reinterpret_cast<char *>(&dataOffset), sizeof(dataOffset));
            in.read(reinterpret_cast<char *>(&dataSize), sizeof(dataSize));
            if (!in.good())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "pak archive index is truncated: " + pakPath.string();
                return false;
            }

            std::string relativePath(pathLength, '\0');
            in.read(relativePath.data(), static_cast<std::streamsize>(relativePath.size()));
            if (in.gcount() != static_cast<std::streamsize>(relativePath.size()))
            {
                if (errorMessage != nullptr)
                    *errorMessage = "pak archive entry path is truncated: " + pakPath.string();
                return false;
            }

            if (!IsSafeRelativePath(std::filesystem::path(relativePath)))
            {
                if (errorMessage != nullptr)
                    *errorMessage = "pak archive contains an unsafe relative path: " + relativePath;
                return false;
            }

            entries.push_back(PakIndexEntry{
                .relativePath = std::move(relativePath),
                .dataOffset = dataOffset,
                .dataSize = dataSize,
            });
        }

        return true;
    }

    std::optional<PakIndexEntry> FindPakEntry(const std::filesystem::path &pakPath,
                                              const std::filesystem::path &relativePath,
                                              std::string *errorMessage)
    {
        std::vector<PakIndexEntry> entries;
        if (!LoadPakIndex(pakPath, entries, errorMessage))
            return std::nullopt;

        const auto wantedPath = PakPathToGenericString(relativePath);
        const auto it = std::find_if(entries.begin(), entries.end(), [&](const PakIndexEntry &entry)
                                     { return entry.relativePath == wantedPath; });
        if (it == entries.end())
            return std::nullopt;

        return *it;
    }

} // namespace

namespace Resource
{
    std::filesystem::path GetPackagedOutputRoot(const std::filesystem::path &rootPath, CookOutputLayout layout)
    {
        switch (layout)
        {
        case CookOutputLayout::Cache:
            return rootPath / "Saved" / "Cache" / "Packaged";
        case CookOutputLayout::Build:
            return rootPath / "build" / "Packaged";
        }

        return rootPath / "Saved" / "Cache" / "Packaged";
    }

    std::filesystem::path GetGamePackagedArchivePath(const std::filesystem::path &packagedRootPath)
    {
        return packagedRootPath / "Game.rtrpak";
    }

    bool BuildPakArchive(const std::filesystem::path &sourceRoot,
                         const std::filesystem::path &pakPath,
                         std::string *errorMessage)
    {
        if (!std::filesystem::exists(sourceRoot))
        {
            if (errorMessage != nullptr)
                *errorMessage = "pak source root does not exist: " + sourceRoot.string();
            return false;
        }

        std::vector<std::filesystem::path> files;
        std::error_code ec;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(sourceRoot, ec))
        {
            if (ec)
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to scan pak source root: " + ec.message();
                return false;
            }

            if (!entry.is_regular_file())
                continue;

            const auto relativePath = std::filesystem::relative(entry.path(), sourceRoot, ec);
            if (ec || !IsSafeRelativePath(relativePath))
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to derive safe relative path for pak entry: " + entry.path().string();
                return false;
            }

            files.push_back(relativePath);
        }

        std::sort(files.begin(), files.end(), [](const auto &lhs, const auto &rhs)
                  { return lhs.generic_string() < rhs.generic_string(); });

        std::filesystem::create_directories(pakPath.parent_path(), ec);
        if (ec)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to create pak output directory: " + ec.message();
            return false;
        }

        std::ofstream out(pakPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open pak archive for writing: " + pakPath.string();
            return false;
        }

        PakHeader header{};
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));

        std::vector<PakIndexEntry> indexEntries;
        indexEntries.reserve(files.size());

        for (const auto &relativePath : files)
        {
            std::ifstream in(sourceRoot / relativePath, std::ios::binary);
            if (!in.is_open())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to open pak source file: " + (sourceRoot / relativePath).string();
                return false;
            }

            const auto dataOffset = static_cast<uint64_t>(out.tellp());
            std::vector<char> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            if (!out.good())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to write pak entry: " + relativePath.string();
                return false;
            }

            indexEntries.push_back(PakIndexEntry{
                .relativePath = PakPathToGenericString(relativePath),
                .dataOffset = dataOffset,
                .dataSize = static_cast<uint64_t>(buffer.size()),
            });
        }

        const auto indexOffset = static_cast<uint64_t>(out.tellp());
        for (const auto &entry : indexEntries)
        {
            const uint32_t pathLength = static_cast<uint32_t>(entry.relativePath.size());
            out.write(reinterpret_cast<const char *>(&pathLength), sizeof(pathLength));
            out.write(reinterpret_cast<const char *>(&entry.dataOffset), sizeof(entry.dataOffset));
            out.write(reinterpret_cast<const char *>(&entry.dataSize), sizeof(entry.dataSize));
            out.write(entry.relativePath.data(), static_cast<std::streamsize>(entry.relativePath.size()));
        }
        if (!out.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to write pak index: " + pakPath.string();
            return false;
        }

        const auto endOffset = static_cast<uint64_t>(out.tellp());
        header.entryCount = static_cast<uint32_t>(indexEntries.size());
        header.indexOffset = indexOffset;
        header.indexSize = endOffset - indexOffset;

        out.seekp(0, std::ios::beg);
        out.write(reinterpret_cast<const char *>(&header), sizeof(header));
        if (!out.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to finalize pak header: " + pakPath.string();
            return false;
        }

        return true;
    }

    std::optional<std::vector<uint8_t>> ReadPakEntry(const std::filesystem::path &pakPath,
                                                     const std::filesystem::path &relativePath,
                                                     std::string *errorMessage)
    {
        const auto entry = FindPakEntry(pakPath, relativePath, errorMessage);
        if (!entry.has_value())
            return std::nullopt;

        std::ifstream in(pakPath, std::ios::binary);
        if (!in.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open pak archive: " + pakPath.string();
            return std::nullopt;
        }

        in.seekg(static_cast<std::streamoff>(entry->dataOffset), std::ios::beg);
        if (!in.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to seek to pak entry: " + entry->relativePath;
            return std::nullopt;
        }

        std::vector<uint8_t> bytes(static_cast<size_t>(entry->dataSize));
        in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (in.gcount() != static_cast<std::streamsize>(bytes.size()))
        {
            if (errorMessage != nullptr)
                *errorMessage = "pak entry payload is truncated: " + entry->relativePath;
            return std::nullopt;
        }

        return bytes;
    }

    bool PakEntryExists(const std::filesystem::path &pakPath,
                        const std::filesystem::path &relativePath,
                        std::string *errorMessage)
    {
        return FindPakEntry(pakPath, relativePath, errorMessage).has_value();
    }

    bool PackageCookedRepositoryCatalogs(const std::filesystem::path &cookedRootPath,
                                         const std::filesystem::path &packagedRootPath,
                                         std::string *errorMessage)
    {
        const auto projectRoot = cookedRootPath / "Project";
        const auto engineRoot = cookedRootPath / "Engine";
        const bool hasProject = std::filesystem::exists(projectRoot);
        const bool hasEngine = std::filesystem::exists(engineRoot);
        if (!hasProject && !hasEngine)
        {
            if (errorMessage != nullptr)
                *errorMessage = "no cooked Project or Engine content found under: " + cookedRootPath.string();
            return false;
        }

        if (hasProject && !std::filesystem::exists(projectRoot / ".rtr" / "catalog.json"))
        {
            if (errorMessage != nullptr)
                *errorMessage = "missing cooked catalog for packaged mount: " + projectRoot.string();
            return false;
        }
        if (hasEngine && !std::filesystem::exists(engineRoot / ".rtr" / "catalog.json"))
        {
            if (errorMessage != nullptr)
                *errorMessage = "missing cooked catalog for packaged mount: " + engineRoot.string();
            return false;
        }

        return BuildMergedGamePakArchive(projectRoot,
                                         engineRoot,
                                         GetGamePackagedArchivePath(packagedRootPath),
                                         errorMessage);
    }
} // namespace Resource
