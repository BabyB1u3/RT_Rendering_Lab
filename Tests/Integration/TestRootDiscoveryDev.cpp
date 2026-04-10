#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Mount/RootDiscovery.h"
#include "RootDiscoveryTestSupport.h"

TEST_F(test_support::RootDiscoveryTestsBase, DiscoverRootPathUsesEnvOverrideWhenProjectMarkerExistsInDev)
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
