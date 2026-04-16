#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Mount/RootDiscovery.h"
#include "Core/Util/CommandLine.h"
#include "RootDiscoveryTestSupport.h"

namespace
{
using RootDiscoveryTests = test_support::RootDiscoveryTestsBase;
}

TEST_F(RootDiscoveryTests, DiscoverRootPathUsesEnvOverrideWhenProjectMarkerExistsInDev)
{
#ifdef RTRLAB_CONFIG_RELEASE
    GTEST_SKIP() << "Dev-profile root discovery coverage runs in non-release builds only.";
#else
    const auto repoRoot = TestRoot() / "Repo";
    test_support::WriteProjectMarkerOrFail(repoRoot);

    const test_support::ScopedEnvVar rootOverride("RTRL_ROOT", repoRoot.string());
    const auto discoveredRoot = Resource::DiscoverRootPath();

    EXPECT_EQ(discoveredRoot, std::filesystem::canonical(repoRoot));
#endif
}

TEST_F(RootDiscoveryTests, DiscoverRootPathPrefersCliOverrideOverEnvInDev)
{
#ifdef RTRLAB_CONFIG_RELEASE
    GTEST_SKIP() << "Dev-profile root discovery coverage runs in non-release builds only.";
#else
    const auto envRepoRoot = TestRoot() / "EnvRepo";
    const auto cliRepoRoot = TestRoot() / "CliRepo";
    test_support::WriteProjectMarkerOrFail(envRepoRoot);
    test_support::WriteProjectMarkerOrFail(cliRepoRoot);

    Util::ParsedCommandLine commandLine;
    commandLine.m_Options.emplace("root", std::string(cliRepoRoot.string()));
    Util::SetProcessCommandLine(commandLine);

    const test_support::ScopedEnvVar rootOverride("RTRL_ROOT", envRepoRoot.string());
    const auto discoveredRoot = Resource::DiscoverRootPath();

    EXPECT_EQ(discoveredRoot, std::filesystem::canonical(cliRepoRoot));
#endif
}
