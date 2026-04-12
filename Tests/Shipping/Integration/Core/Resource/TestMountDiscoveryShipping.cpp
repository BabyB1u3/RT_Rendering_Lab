#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Resource/Catalog/ResourceCatalog.h"
#include "Core/Resource/Mount/MountBackend.h"
#include "Core/Resource/Package/PakArchive.h"
#include "RootDiscoveryTestSupport.h"
#include "ResourceTestSupport.h"

namespace
{
    using MountDiscoveryShippingTests = test_support::RootDiscoveryTestsBase;

    struct ArchiveEntry
    {
        std::string logicalPath;
        std::string relativePath;
        std::string contents;
    };

    void BuildSharedGameArchiveFixture(const std::filesystem::path &repoRoot)
    {
        const auto cookedRoot = test_support::CookedRoot(repoRoot);

        test_support::WriteTextFileOrFail(
            test_support::ProjectCookedCatalogPath(cookedRoot),
            "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": \"/Project/Textures/Grassy_Square\",\n      \"artifacts\": [\n        {\n          \"relativePath\": \"Textures/Grassy_Square.rtrtex\",\n          \"format\": \"rtrtex\",\n          \"profileTag\": \"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    }\n  ]\n}\n");
        test_support::WriteProjectCookedFileOrFail(cookedRoot, "Textures/Grassy_Square.rtrtex", "project");

        test_support::WriteTextFileOrFail(
            test_support::EngineCookedCatalogPath(cookedRoot),
            "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": \"/Engine/Defaults/Materials/ErrorMaterial\",\n      \"artifacts\": [\n        {\n          \"relativePath\": \"Defaults/Materials/ErrorMaterial.json\",\n          \"format\": \"json\",\n          \"profileTag\": \"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    }\n  ]\n}\n");
        test_support::WriteEngineCookedFileOrFail(cookedRoot, "Defaults/Materials/ErrorMaterial.json", "engine");

        std::string errorMessage;
        ASSERT_TRUE(Resource::PackageCookedRepositoryCatalogs(cookedRoot, repoRoot, &errorMessage)) << errorMessage;
    }

    void BuildArchiveFixture(const std::filesystem::path &archiveRoot,
                             const std::filesystem::path &archivePath,
                             const std::vector<ArchiveEntry> &entries)
    {
        std::string catalog = "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n";
        for (size_t i = 0; i < entries.size(); ++i)
        {
            const auto &entry = entries[i];
            if (i != 0)
                catalog += ",\n";

            catalog +=
                "    {\n"
                "      \"logicalPath\": \"" + entry.logicalPath + "\",\n"
                "      \"artifacts\": [\n"
                "        {\n"
                "          \"relativePath\": \"" + entry.relativePath + "\",\n"
                "          \"format\": \"bin\",\n"
                "          \"profileTag\": \"cooked\",\n"
                "          \"backendTag\": \"any\",\n"
                "          \"platformTag\": \"any\"\n"
                "        }\n"
                "      ]\n"
                "    }";
        }
        catalog += "\n  ]\n}\n";

        test_support::WriteTextFileOrFail(test_support::MountCatalogPath(archiveRoot), catalog);
        for (const auto &entry : entries)
            test_support::WriteMountFileOrFail(archiveRoot, entry.relativePath, entry.contents);

        std::string errorMessage;
        ASSERT_TRUE(Resource::BuildPakArchive(archiveRoot, archivePath, &errorMessage)) << errorMessage;
    }

    bool ContainsMount(const std::vector<Resource::ReadableMount> &mounts,
                       std::string_view sourceKey,
                       Resource::MountPriority priority,
                       const std::filesystem::path &mountRoot)
    {
        return std::any_of(mounts.begin(), mounts.end(), [&](const auto &mount) {
            return mount.sourceKey == sourceKey &&
                   mount.priority == priority &&
                   mount.backend == Resource::MountBackendKind::PakArchive &&
                   mount.mountRoot == mountRoot;
        });
    }
} // namespace

TEST_F(MountDiscoveryShippingTests, DiscoverReadableMountBackendsUsesGameArchiveForShippingProfile)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), repoRoot / "Saved" / "Cache", "Project", "shipping");

    ASSERT_EQ(mounts.size(), 2u);
    EXPECT_EQ(mounts[0].sourceKey, "Project");
    EXPECT_EQ(mounts[0].priority, Resource::MountPriority::Packaged);
    EXPECT_EQ(mounts[0].backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[0].mountRoot, test_support::GamePackagedArchivePath(repoRoot));
    EXPECT_EQ(mounts[1].sourceKey, "Engine");
    EXPECT_EQ(mounts[1].priority, Resource::MountPriority::Packaged);
    EXPECT_EQ(mounts[1].backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[1].mountRoot, test_support::GamePackagedArchivePath(repoRoot));
}

TEST_F(MountDiscoveryShippingTests, DiscoverReadableMountBackendsFindsDlcPatchAndModArchives)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    BuildArchiveFixture(
        repoRoot / "DLC" / "Expansion1_Source",
        repoRoot / "DLC" / "Expansion1.rtrpak",
        {
            ArchiveEntry{
                .logicalPath = "/Project/Textures/Grassy_Square",
                .relativePath = "Project/Textures/Grassy_Square.rtrtex",
                .contents = "dlc",
            },
            ArchiveEntry{
                .logicalPath = "/DLC/Expansion1/Weapons/LaserRifle",
                .relativePath = "DLC/Expansion1/Weapons/LaserRifle.bin",
                .contents = "laser",
            },
        });
    BuildArchiveFixture(
        repoRoot / "Patches" / "Patch_001_Source",
        repoRoot / "Patches" / "Patch_001.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/Project/Textures/Grassy_Square",
            .relativePath = "Project/Textures/Grassy_Square.rtrtex",
            .contents = "patch",
        }});
    BuildArchiveFixture(
        repoRoot / "Mods" / "CoolMod_Source",
        repoRoot / "Mods" / "CoolMod.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/Project/Textures/Grassy_Square",
            .relativePath = "Project/Textures/Grassy_Square.rtrtex",
            .contents = "mod",
        }});

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), repoRoot / "Saved" / "Cache", "Project", "shipping");

    ASSERT_EQ(mounts.size(), 10u);
    EXPECT_TRUE(ContainsMount(
        mounts,
        "DLC:Expansion1:Project",
        Resource::MountPriority::DLC,
        repoRoot / "DLC" / "Expansion1.rtrpak"));
    EXPECT_TRUE(ContainsMount(
        mounts,
        "DLC:Expansion1",
        Resource::MountPriority::DLC,
        repoRoot / "DLC" / "Expansion1.rtrpak"));
    EXPECT_TRUE(ContainsMount(
        mounts,
        "Patch:Patch_001:Project",
        Resource::MountPriority::Patch,
        repoRoot / "Patches" / "Patch_001.rtrpak"));
    EXPECT_TRUE(ContainsMount(
        mounts,
        "Mod:CoolMod:Project",
        Resource::MountPriority::Mod,
        repoRoot / "Mods" / "CoolMod.rtrpak"));
    EXPECT_TRUE(ContainsMount(
        mounts,
        "Mod:CoolMod",
        Resource::MountPriority::Mod,
        repoRoot / "Mods" / "CoolMod.rtrpak"));
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryPrefersModOverPatchDlcAndBaseInShippingProfile)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    BuildArchiveFixture(
        repoRoot / "DLC" / "Expansion1_Source",
        repoRoot / "DLC" / "Expansion1.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/Project/Textures/Grassy_Square",
            .relativePath = "Project/Textures/Grassy_Square.rtrtex",
            .contents = "dlc",
        }});
    BuildArchiveFixture(
        repoRoot / "Patches" / "Patch_001_Source",
        repoRoot / "Patches" / "Patch_001.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/Project/Textures/Grassy_Square",
            .relativePath = "Project/Textures/Grassy_Square.rtrtex",
            .contents = "patch",
        }});
    BuildArchiveFixture(
        repoRoot / "Mods" / "CoolMod_Source",
        repoRoot / "Mods" / "CoolMod.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/Project/Textures/Grassy_Square",
            .relativePath = "Project/Textures/Grassy_Square.rtrtex",
            .contents = "mod",
        }});

    Resource::CatalogRegistry registry;

    const auto projectVirtualPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Textures/Grassy_Square"};
    const auto projectResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        projectVirtualPath,
        "/Project/Textures/Grassy_Square",
        "Project");
    ASSERT_TRUE(projectResolved.has_value());
    EXPECT_EQ(projectResolved->backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(projectResolved->mountRoot, repoRoot / "Mods" / "CoolMod.rtrpak");
    EXPECT_EQ(projectResolved->relativePath, std::filesystem::path("Project/Textures/Grassy_Square.rtrtex"));

    std::string errorMessage;
    const auto projectBytes = Resource::ReadReadableArtifactBinary(*projectResolved, &errorMessage);
    ASSERT_TRUE(projectBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(projectBytes->begin(), projectBytes->end()), "mod");
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryResolvesDlcAndModNamespaceEntriesFromOverlayArchives)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    BuildArchiveFixture(
        repoRoot / "DLC" / "Expansion1_Source",
        repoRoot / "DLC" / "Expansion1.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/DLC/Expansion1/Weapons/LaserRifle",
            .relativePath = "DLC/Expansion1/Weapons/LaserRifle.bin",
            .contents = "laser",
        }});
    BuildArchiveFixture(
        repoRoot / "Mods" / "CoolMod_Source",
        repoRoot / "Mods" / "CoolMod.rtrpak",
        {ArchiveEntry{
            .logicalPath = "/Mod/CoolMod/Weapons/Hammer",
            .relativePath = "Mod/CoolMod/Weapons/Hammer.bin",
            .contents = "hammer",
        }});

    Resource::CatalogRegistry registry;
    std::string errorMessage;

    const auto dlcVirtualPath = Resource::VirtualPath{Resource::PathDomain::DLC, std::string("Expansion1"), "Weapons/LaserRifle"};
    const auto dlcResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        dlcVirtualPath,
        "/DLC/Expansion1/Weapons/LaserRifle",
        "Project");
    ASSERT_TRUE(dlcResolved.has_value());
    EXPECT_EQ(dlcResolved->mountRoot, repoRoot / "DLC" / "Expansion1.rtrpak");
    EXPECT_EQ(dlcResolved->relativePath, std::filesystem::path("DLC/Expansion1/Weapons/LaserRifle.bin"));
    const auto dlcBytes = Resource::ReadReadableArtifactBinary(*dlcResolved, &errorMessage);
    ASSERT_TRUE(dlcBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(dlcBytes->begin(), dlcBytes->end()), "laser");

    const auto modVirtualPath = Resource::VirtualPath{Resource::PathDomain::Mod, std::string("CoolMod"), "Weapons/Hammer"};
    const auto modResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        modVirtualPath,
        "/Mod/CoolMod/Weapons/Hammer",
        "Project");
    ASSERT_TRUE(modResolved.has_value());
    EXPECT_EQ(modResolved->mountRoot, repoRoot / "Mods" / "CoolMod.rtrpak");
    EXPECT_EQ(modResolved->relativePath, std::filesystem::path("Mod/CoolMod/Weapons/Hammer.bin"));
    const auto modBytes = Resource::ReadReadableArtifactBinary(*modResolved, &errorMessage);
    ASSERT_TRUE(modBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(modBytes->begin(), modBytes->end()), "hammer");
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryResolvesProjectAndEngineEntriesFromSharedGameArchive)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    Resource::CatalogRegistry registry;

    const auto projectVirtualPath = Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Textures/Grassy_Square"};
    const auto projectResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        projectVirtualPath,
        "/Project/Textures/Grassy_Square",
        "Project");
    ASSERT_TRUE(projectResolved.has_value());
    EXPECT_EQ(projectResolved->backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(projectResolved->mountRoot, test_support::GamePackagedArchivePath(repoRoot));
    EXPECT_EQ(projectResolved->relativePath, std::filesystem::path("Project/Textures/Grassy_Square.rtrtex"));
    std::string errorMessage;
    const auto projectBytes = Resource::ReadReadableArtifactBinary(*projectResolved, &errorMessage);
    ASSERT_TRUE(projectBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(projectBytes->begin(), projectBytes->end()), "project");

    const auto engineVirtualPath = Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, "Defaults/Materials/ErrorMaterial"};
    const auto engineResolved = registry.ResolveArtifact(
        repoRoot,
        test_support::EngineRoot(repoRoot),
        repoRoot / "Saved" / "Cache",
        engineVirtualPath,
        "/Engine/Defaults/Materials/ErrorMaterial",
        "Project");
    ASSERT_TRUE(engineResolved.has_value());
    EXPECT_EQ(engineResolved->backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(engineResolved->mountRoot, test_support::GamePackagedArchivePath(repoRoot));
    EXPECT_EQ(engineResolved->relativePath, std::filesystem::path("Engine/Defaults/Materials/ErrorMaterial.json"));
    const auto engineText = Resource::ReadReadableArtifactText(*engineResolved, &errorMessage);
    ASSERT_TRUE(engineText.has_value()) << errorMessage;
    EXPECT_EQ(*engineText, "engine");
}

