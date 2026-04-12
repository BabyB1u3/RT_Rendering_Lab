#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Mount/RootDiscovery.h"
#include "Core/Util/CommandLine.h"
#include "RootDiscoveryTestSupport.h"

namespace
{
    using RootDiscoveryTests = test_support::RootDiscoveryTestsBase;
}

TEST_F(RootDiscoveryTests, DiscoverRootPathIgnoresEnvOverrideInShipping)
{
#ifndef RTRLAB_CONFIG_RELEASE
    GTEST_SKIP() << "Shipping-profile root discovery coverage runs in release builds only.";
#else
    const auto repoRoot = TestRoot() / "Repo";
    test_support::WriteProjectMarkerOrFail(repoRoot);

    const test_support::ScopedEnvVar rootOverride("RTRL_ROOT", repoRoot.string());
    const auto discoveredRoot = Resource::DiscoverRootPath();
    const auto executableDirectory = test_support::GetExecutableDirectoryForTest();

    ASSERT_FALSE(executableDirectory.empty());
    EXPECT_EQ(discoveredRoot, std::filesystem::canonical(executableDirectory));
#endif
}

TEST_F(RootDiscoveryTests, DiscoverRootPathIgnoresCliOverrideWithoutDevModeInShipping)
{
#ifndef RTRLAB_CONFIG_RELEASE
    GTEST_SKIP() << "Shipping-profile root discovery coverage runs in release builds only.";
#else
    const auto repoRoot = TestRoot() / "Repo";
    test_support::WriteProjectMarkerOrFail(repoRoot);

    Util::ParsedCommandLine commandLine;
    commandLine.options.emplace("root", std::string(repoRoot.string()));
    Util::SetProcessCommandLine(commandLine);

    const auto discoveredRoot = Resource::DiscoverRootPath();
    const auto executableDirectory = test_support::GetExecutableDirectoryForTest();

    ASSERT_FALSE(executableDirectory.empty());
    EXPECT_EQ(discoveredRoot, std::filesystem::canonical(executableDirectory));
#endif
}

TEST_F(RootDiscoveryTests, DiscoverRootPathUsesCliOverrideWhenDevModeIsEnabledInShipping)
{
#ifndef RTRLAB_CONFIG_RELEASE
    GTEST_SKIP() << "Shipping-profile root discovery coverage runs in release builds only.";
#else
    const auto repoRoot = TestRoot() / "Repo";
    test_support::WriteProjectMarkerOrFail(repoRoot);

    Util::ParsedCommandLine commandLine;
    commandLine.options.emplace("dev-mode", std::nullopt);
    commandLine.options.emplace("root", std::string(repoRoot.string()));
    Util::SetProcessCommandLine(commandLine);

    const auto discoveredRoot = Resource::DiscoverRootPath();
    EXPECT_EQ(discoveredRoot, std::filesystem::canonical(repoRoot));
#endif
}
