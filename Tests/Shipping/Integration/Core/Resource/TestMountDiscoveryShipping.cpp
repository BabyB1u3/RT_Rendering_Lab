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
    std::string m_LogicalPath;
    std::string m_RelativePath;
    std::string m_Contents;
};

void BuildSharedGameArchiveFixture(const std::filesystem::path& repoRoot)
{
    const auto cookedRoot = test_support::CookedRoot(repoRoot);

    test_support::WriteTextFileOrFail(
        test_support::ProjectCookedCatalogPath(cookedRoot),
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": "
        "\"/Project/Textures/Grassy_Square\",\n      \"artifacts\": [\n        {\n          \"relativePath\": "
        "\"Textures/Grassy_Square.rtrtex\",\n          \"format\": \"rtrtex\",\n          \"profileTag\": "
        "\"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    "
        "}\n  ]\n}\n");
    test_support::WriteProjectCookedFileOrFail(cookedRoot, "Textures/Grassy_Square.rtrtex", "project");

    test_support::WriteTextFileOrFail(
        test_support::EngineCookedCatalogPath(cookedRoot),
        "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n    {\n      \"logicalPath\": "
        "\"/Engine/Defaults/Materials/ErrorMaterial\",\n      \"artifacts\": [\n        {\n          \"relativePath\": "
        "\"Defaults/Materials/ErrorMaterial.json\",\n          \"format\": \"json\",\n          \"profileTag\": "
        "\"cooked\",\n          \"backendTag\": \"any\",\n          \"platformTag\": \"any\"\n        }\n      ]\n    "
        "}\n  ]\n}\n");
    test_support::WriteEngineCookedFileOrFail(cookedRoot, "Defaults/Materials/ErrorMaterial.json", "engine");

    std::string errorMessage;
    ASSERT_TRUE(Resource::PackageCookedRepositoryCatalogs(cookedRoot, repoRoot, &errorMessage)) << errorMessage;
}

void BuildArchiveFixture(const std::filesystem::path& archiveRoot,
                         const std::filesystem::path& archivePath,
                         const std::vector<ArchiveEntry>& entries)
{
    std::string catalog = "{\n  \"version\": 2,\n  \"kind\": \"cooked\",\n  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        if (i != 0)
            catalog += ",\n";

        catalog += "    {\n"
                   "      \"logicalPath\": \"" +
                   entry.m_LogicalPath +
                   "\",\n"
                   "      \"artifacts\": [\n"
                   "        {\n"
                   "          \"relativePath\": \"" +
                   entry.m_RelativePath +
                   "\",\n"
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
    for (const auto& entry : entries)
        test_support::WriteMountFileOrFail(archiveRoot, entry.m_RelativePath, entry.m_Contents);

    std::string errorMessage;
    ASSERT_TRUE(Resource::BuildPakArchive(archiveRoot, archivePath, &errorMessage)) << errorMessage;
}

bool ContainsMount(const std::vector<Resource::ReadableMount>& mounts,
                   std::string_view sourceKey,
                   Resource::MountPriority priority,
                   const std::filesystem::path& mountRoot)
{
    return std::any_of(mounts.begin(),
                       mounts.end(),
                       [&](const auto& mount)
                       {
                           return mount.m_SourceKey == sourceKey && mount.m_Priority == priority &&
                                  mount.m_Backend == Resource::MountBackendKind::PakArchive &&
                                  mount.m_MountRoot == mountRoot;
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
    EXPECT_EQ(mounts[0].m_SourceKey, "Project");
    EXPECT_EQ(mounts[0].m_Priority, Resource::MountPriority::Packaged);
    EXPECT_EQ(mounts[0].m_Backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[0].m_MountRoot, test_support::GamePackagedArchivePath(repoRoot));
    EXPECT_EQ(mounts[1].m_SourceKey, "Engine");
    EXPECT_EQ(mounts[1].m_Priority, Resource::MountPriority::Packaged);
    EXPECT_EQ(mounts[1].m_Backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(mounts[1].m_MountRoot, test_support::GamePackagedArchivePath(repoRoot));
}

TEST_F(MountDiscoveryShippingTests, DiscoverReadableMountBackendsFindsDlcPatchAndModArchives)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    BuildArchiveFixture(repoRoot / "DLC" / "Expansion1_Source",
                        repoRoot / "DLC" / "Expansion1.rtrpak",
                        {
                            ArchiveEntry{
                                .m_LogicalPath = "/Project/Textures/Grassy_Square",
                                .m_RelativePath = "Project/Textures/Grassy_Square.rtrtex",
                                .m_Contents = "dlc",
                            },
                            ArchiveEntry{
                                .m_LogicalPath = "/DLC/Expansion1/Weapons/LaserRifle",
                                .m_RelativePath = "DLC/Expansion1/Weapons/LaserRifle.bin",
                                .m_Contents = "laser",
                            },
                        });
    BuildArchiveFixture(repoRoot / "Patches" / "Patch_001_Source",
                        repoRoot / "Patches" / "Patch_001.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/Project/Textures/Grassy_Square",
                            .m_RelativePath = "Project/Textures/Grassy_Square.rtrtex",
                            .m_Contents = "patch",
                        }});
    BuildArchiveFixture(repoRoot / "Mods" / "CoolMod_Source",
                        repoRoot / "Mods" / "CoolMod.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/Project/Textures/Grassy_Square",
                            .m_RelativePath = "Project/Textures/Grassy_Square.rtrtex",
                            .m_Contents = "mod",
                        }});

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), repoRoot / "Saved" / "Cache", "Project", "shipping");

    ASSERT_EQ(mounts.size(), 10u);
    EXPECT_TRUE(ContainsMount(
        mounts, "DLC:Expansion1:Project", Resource::MountPriority::DLC, repoRoot / "DLC" / "Expansion1.rtrpak"));
    EXPECT_TRUE(
        ContainsMount(mounts, "DLC:Expansion1", Resource::MountPriority::DLC, repoRoot / "DLC" / "Expansion1.rtrpak"));
    EXPECT_TRUE(ContainsMount(
        mounts, "Patch:Patch_001:Project", Resource::MountPriority::Patch, repoRoot / "Patches" / "Patch_001.rtrpak"));
    EXPECT_TRUE(ContainsMount(
        mounts, "Mod:CoolMod:Project", Resource::MountPriority::Mod, repoRoot / "Mods" / "CoolMod.rtrpak"));
    EXPECT_TRUE(
        ContainsMount(mounts, "Mod:CoolMod", Resource::MountPriority::Mod, repoRoot / "Mods" / "CoolMod.rtrpak"));
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryPrefersModOverPatchDlcAndBaseInShippingProfile)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    BuildArchiveFixture(repoRoot / "DLC" / "Expansion1_Source",
                        repoRoot / "DLC" / "Expansion1.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/Project/Textures/Grassy_Square",
                            .m_RelativePath = "Project/Textures/Grassy_Square.rtrtex",
                            .m_Contents = "dlc",
                        }});
    BuildArchiveFixture(repoRoot / "Patches" / "Patch_001_Source",
                        repoRoot / "Patches" / "Patch_001.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/Project/Textures/Grassy_Square",
                            .m_RelativePath = "Project/Textures/Grassy_Square.rtrtex",
                            .m_Contents = "patch",
                        }});
    BuildArchiveFixture(repoRoot / "Mods" / "CoolMod_Source",
                        repoRoot / "Mods" / "CoolMod.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/Project/Textures/Grassy_Square",
                            .m_RelativePath = "Project/Textures/Grassy_Square.rtrtex",
                            .m_Contents = "mod",
                        }});

    Resource::CatalogRegistry registry;

    const auto projectVirtualPath =
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Textures/Grassy_Square"};
    const auto projectResolved = registry.ResolveArtifact(repoRoot,
                                                          test_support::EngineRoot(repoRoot),
                                                          repoRoot / "Saved" / "Cache",
                                                          projectVirtualPath,
                                                          "/Project/Textures/Grassy_Square",
                                                          "Project");
    ASSERT_TRUE(projectResolved.has_value());
    EXPECT_EQ(projectResolved->m_Backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(projectResolved->m_MountRoot, repoRoot / "Mods" / "CoolMod.rtrpak");
    EXPECT_EQ(projectResolved->m_RelativePath, std::filesystem::path("Project/Textures/Grassy_Square.rtrtex"));

    std::string errorMessage;
    const auto projectBytes = Resource::ReadReadableArtifactBinary(*projectResolved, &errorMessage);
    ASSERT_TRUE(projectBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(projectBytes->begin(), projectBytes->end()), "mod");
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryResolvesDlcAndModNamespaceEntriesFromOverlayArchives)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    BuildArchiveFixture(repoRoot / "DLC" / "Expansion1_Source",
                        repoRoot / "DLC" / "Expansion1.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/DLC/Expansion1/Weapons/LaserRifle",
                            .m_RelativePath = "DLC/Expansion1/Weapons/LaserRifle.bin",
                            .m_Contents = "laser",
                        }});
    BuildArchiveFixture(repoRoot / "Mods" / "CoolMod_Source",
                        repoRoot / "Mods" / "CoolMod.rtrpak",
                        {ArchiveEntry{
                            .m_LogicalPath = "/Mod/CoolMod/Weapons/Hammer",
                            .m_RelativePath = "Mod/CoolMod/Weapons/Hammer.bin",
                            .m_Contents = "hammer",
                        }});

    Resource::CatalogRegistry registry;
    std::string errorMessage;

    const auto dlcVirtualPath =
        Resource::VirtualPath{Resource::PathDomain::DLC, std::string("Expansion1"), "Weapons/LaserRifle"};
    const auto dlcResolved = registry.ResolveArtifact(repoRoot,
                                                      test_support::EngineRoot(repoRoot),
                                                      repoRoot / "Saved" / "Cache",
                                                      dlcVirtualPath,
                                                      "/DLC/Expansion1/Weapons/LaserRifle",
                                                      "Project");
    ASSERT_TRUE(dlcResolved.has_value());
    EXPECT_EQ(dlcResolved->m_MountRoot, repoRoot / "DLC" / "Expansion1.rtrpak");
    EXPECT_EQ(dlcResolved->m_RelativePath, std::filesystem::path("DLC/Expansion1/Weapons/LaserRifle.bin"));
    const auto dlcBytes = Resource::ReadReadableArtifactBinary(*dlcResolved, &errorMessage);
    ASSERT_TRUE(dlcBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(dlcBytes->begin(), dlcBytes->end()), "laser");

    const auto modVirtualPath =
        Resource::VirtualPath{Resource::PathDomain::Mod, std::string("CoolMod"), "Weapons/Hammer"};
    const auto modResolved = registry.ResolveArtifact(repoRoot,
                                                      test_support::EngineRoot(repoRoot),
                                                      repoRoot / "Saved" / "Cache",
                                                      modVirtualPath,
                                                      "/Mod/CoolMod/Weapons/Hammer",
                                                      "Project");
    ASSERT_TRUE(modResolved.has_value());
    EXPECT_EQ(modResolved->m_MountRoot, repoRoot / "Mods" / "CoolMod.rtrpak");
    EXPECT_EQ(modResolved->m_RelativePath, std::filesystem::path("Mod/CoolMod/Weapons/Hammer.bin"));
    const auto modBytes = Resource::ReadReadableArtifactBinary(*modResolved, &errorMessage);
    ASSERT_TRUE(modBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(modBytes->begin(), modBytes->end()), "hammer");
}

TEST_F(MountDiscoveryShippingTests, CatalogRegistryResolvesProjectAndEngineEntriesFromSharedGameArchive)
{
    const auto repoRoot = test_support::CreateRepoRootOrFail(TestRoot());
    BuildSharedGameArchiveFixture(repoRoot);

    Resource::CatalogRegistry registry;

    const auto projectVirtualPath =
        Resource::VirtualPath{Resource::PathDomain::Project, std::nullopt, "Textures/Grassy_Square"};
    const auto projectResolved = registry.ResolveArtifact(repoRoot,
                                                          test_support::EngineRoot(repoRoot),
                                                          repoRoot / "Saved" / "Cache",
                                                          projectVirtualPath,
                                                          "/Project/Textures/Grassy_Square",
                                                          "Project");
    ASSERT_TRUE(projectResolved.has_value());
    EXPECT_EQ(projectResolved->m_Backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(projectResolved->m_MountRoot, test_support::GamePackagedArchivePath(repoRoot));
    EXPECT_EQ(projectResolved->m_RelativePath, std::filesystem::path("Project/Textures/Grassy_Square.rtrtex"));
    std::string errorMessage;
    const auto projectBytes = Resource::ReadReadableArtifactBinary(*projectResolved, &errorMessage);
    ASSERT_TRUE(projectBytes.has_value()) << errorMessage;
    EXPECT_EQ(std::string(projectBytes->begin(), projectBytes->end()), "project");

    const auto engineVirtualPath =
        Resource::VirtualPath{Resource::PathDomain::Engine, std::nullopt, "Defaults/Materials/ErrorMaterial"};
    const auto engineResolved = registry.ResolveArtifact(repoRoot,
                                                         test_support::EngineRoot(repoRoot),
                                                         repoRoot / "Saved" / "Cache",
                                                         engineVirtualPath,
                                                         "/Engine/Defaults/Materials/ErrorMaterial",
                                                         "Project");
    ASSERT_TRUE(engineResolved.has_value());
    EXPECT_EQ(engineResolved->m_Backend, Resource::MountBackendKind::PakArchive);
    EXPECT_EQ(engineResolved->m_MountRoot, test_support::GamePackagedArchivePath(repoRoot));
    EXPECT_EQ(engineResolved->m_RelativePath, std::filesystem::path("Engine/Defaults/Materials/ErrorMaterial.json"));
    const auto engineText = Resource::ReadReadableArtifactText(*engineResolved, &errorMessage);
    ASSERT_TRUE(engineText.has_value()) << errorMessage;
    EXPECT_EQ(*engineText, "engine");
}
