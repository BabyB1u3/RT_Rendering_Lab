#include "Core/Resource/Catalog/SourceCatalog.h"

#include "Core/Resource/Path/PathParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <json.hpp>
#include <unordered_map>

namespace
{
    using Json = nlohmann::json;

    std::string CatalogPathToGenericString(const std::filesystem::path &path)
    {
        return path.generic_string();
    }

    std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    bool IsExcludedRelativePath(const std::filesystem::path &relativePath)
    {
        bool firstSegment = true;
        for (const auto &segment : relativePath)
        {
            const auto segmentText = segment.generic_string();
            if (segmentText == ".rtr")
                return true;
            if (firstSegment && segmentText == "Config")
                return true;
            firstSegment = false;
        }

        return false;
    }

    std::string MakeLogicalPath(const Resource::VirtualPath &mountPath, const std::filesystem::path &relativeWithoutExtension)
    {
        static const std::unordered_map<std::string, std::string> kCanonicalSegmentNames{
            {"textures", "Textures"},
            {"shaders", "Shaders"},
            {"materials", "Materials"},
            {"scenes", "Scenes"},
            {"defaults", "Defaults"},
            {"editor", "Editor"},
            {"config", "Config"},
        };

        std::filesystem::path canonicalRelative;
        for (const auto &segment : relativeWithoutExtension)
        {
            const auto segmentText = segment.generic_string();
            const auto lookupIt = kCanonicalSegmentNames.find(ToLowerAscii(segmentText));
            canonicalRelative /= (lookupIt != kCanonicalSegmentNames.end()) ? lookupIt->second : segmentText;
        }

        const auto relative = CatalogPathToGenericString(canonicalRelative);

        switch (mountPath.domain)
        {
        case Resource::PathDomain::Project:
            return relative.empty() ? "/Project" : "/Project/" + relative;
        case Resource::PathDomain::Engine:
            return relative.empty() ? "/Engine" : "/Engine/" + relative;
        case Resource::PathDomain::Saved:
            return relative.empty() ? "/Saved" : "/Saved/" + relative;
        case Resource::PathDomain::Cache:
            return relative.empty() ? "/Cache" : "/Cache/" + relative;
        }

        return {};
    }

    std::string DetectFormat(const std::filesystem::path &path)
    {
        auto extension = path.extension().string();
        if (!extension.empty() && extension.front() == '.')
            extension.erase(extension.begin());
        return ToLowerAscii(extension);
    }

    std::vector<std::pair<Resource::VirtualPath, std::filesystem::path>> DiscoverReadableSourceMounts(
        const std::filesystem::path &rootPath,
        std::string_view projectContentDirName)
    {
        std::vector<std::pair<Resource::VirtualPath, std::filesystem::path>> mounts;

        const auto projectRoot = rootPath / projectContentDirName;
        if (std::filesystem::exists(projectRoot))
            mounts.emplace_back(Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, {}}, projectRoot);

        const auto engineRoot = rootPath / "EngineContent";
        if (std::filesystem::exists(engineRoot))
            mounts.emplace_back(Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, {}}, engineRoot);

        return mounts;
    }
} // namespace

namespace Resource
{
    bool BuildSourceCatalogEntries(const std::filesystem::path &mountRoot,
                                   const VirtualPath &mountPath,
                                   std::vector<ResourceCatalogEntry> &entries,
                                   std::string *errorMessage)
    {
        entries.clear();

        std::unordered_map<std::string, size_t> entryIndexByLogicalPath;
        std::error_code ec;
        std::vector<std::filesystem::path> files;

        if (!std::filesystem::exists(mountRoot))
        {
            if (errorMessage != nullptr)
                *errorMessage = "mount root does not exist: " + mountRoot.string();
            return false;
        }

        for (const auto &entry : std::filesystem::recursive_directory_iterator(mountRoot, ec))
        {
            if (ec)
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to scan mount root: " + ec.message();
                return false;
            }

            if (!entry.is_regular_file())
                continue;

            const auto relativePath = std::filesystem::relative(entry.path(), mountRoot, ec);
            if (ec)
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to compute relative path for " + entry.path().string();
                return false;
            }

            if (IsExcludedRelativePath(relativePath))
                continue;

            files.push_back(relativePath);
        }

        std::sort(files.begin(), files.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.generic_string() < rhs.generic_string();
        });

        for (const auto &relativePath : files)
        {
            const auto logicalRelativePath = relativePath.parent_path() / relativePath.stem();
            const auto logicalPath = MakeLogicalPath(mountPath, logicalRelativePath);

            if (logicalPath.empty())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "failed to derive logical path for " + relativePath.string();
                return false;
            }

            if (entryIndexByLogicalPath.contains(logicalPath))
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = "duplicate logical path '" + logicalPath +
                                    "' generated from source file '" + relativePath.generic_string() + "'";
                }
                return false;
            }

            ResourceCatalogEntry catalogEntry;
            catalogEntry.logicalPath = logicalPath;
            catalogEntry.sourceRelativePath = CatalogPathToGenericString(relativePath);
            catalogEntry.artifacts.push_back(ArtifactRecord{
                .relativePath = CatalogPathToGenericString(relativePath),
                .format = DetectFormat(relativePath),
                .platformTag = "any",
                .backendTag = "any",
                .profileTag = "dev",
                .contentHash = 0,
            });

            entryIndexByLogicalPath.emplace(logicalPath, entries.size());
            entries.push_back(std::move(catalogEntry));
        }

        return true;
    }

    bool WriteSourceCatalogJson(const std::filesystem::path &catalogPath,
                                const std::vector<ResourceCatalogEntry> &entries,
                                std::string *errorMessage)
    {
        Json root;
        root["version"] = 1;
        root["entries"] = Json::array();

        for (const auto &entry : entries)
        {
            if (!entry.sourceRelativePath.has_value())
            {
                if (errorMessage != nullptr)
                    *errorMessage = "source catalog entry is missing sourceRelativePath: " + entry.logicalPath;
                return false;
            }

            Json entryJson;
            entryJson["logicalPath"] = entry.logicalPath;
            entryJson["sourceRelativePath"] = *entry.sourceRelativePath;
            entryJson["artifacts"] = Json::array();

            for (const auto &artifact : entry.artifacts)
            {
                Json artifactJson;
                artifactJson["relativePath"] = artifact.relativePath;
                artifactJson["format"] = artifact.format;
                artifactJson["platformTag"] = artifact.platformTag;
                artifactJson["backendTag"] = artifact.backendTag;
                artifactJson["profileTag"] = artifact.profileTag;
                if (artifact.contentHash != 0)
                    artifactJson["contentHash"] = artifact.contentHash;
                entryJson["artifacts"].push_back(std::move(artifactJson));
            }

            root["entries"].push_back(std::move(entryJson));
        }

        std::error_code ec;
        std::filesystem::create_directories(catalogPath.parent_path(), ec);
        if (ec)
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to create catalog directory: " + ec.message();
            return false;
        }

        std::ofstream out(catalogPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!out.is_open())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to open catalog for writing: " + catalogPath.string();
            return false;
        }

        out << root.dump(2) << '\n';
        if (!out.good())
        {
            if (errorMessage != nullptr)
                *errorMessage = "failed to write catalog: " + catalogPath.string();
            return false;
        }

        return true;
    }

    bool IndexRepositorySourceCatalogs(const std::filesystem::path &rootPath,
                                       std::string_view projectContentDirName,
                                       std::string *errorMessage)
    {
        for (const auto &[mountPath, mountRoot] : DiscoverReadableSourceMounts(rootPath, projectContentDirName))
        {
            std::vector<ResourceCatalogEntry> entries;
            if (!BuildSourceCatalogEntries(mountRoot, mountPath, entries, errorMessage))
                return false;

            if (!WriteSourceCatalogJson(mountRoot / ".rtr" / "catalog.json", entries, errorMessage))
                return false;
        }

        return true;
    }
} // namespace Resource
