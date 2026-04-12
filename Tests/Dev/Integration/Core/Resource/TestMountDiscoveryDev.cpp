#include <gtest/gtest.h>

#include <filesystem>

#include "Core/Resource/Mount/MountBackend.h"
#include "ResourceTestSupport.h"

namespace
{
    class MountDiscoveryDevTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = test_support::CurrentTestRoot("mount-discovery-dev");
            test_support::ResetCurrentTestRoot("mount-discovery-dev");
        }

        void TearDown() override
        {
            test_support::RemoveCurrentTestArtifacts("mount-discovery-dev");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(MountDiscoveryDevTests, DiscoverReadableMountBackendsFindsSourceDirectoryMounts)
{
    const auto repoRoot = TestRoot() / "repo";
    test_support::WriteProjectMarkerOrFail(repoRoot);

    test_support::EnsureDirectories(test_support::ProjectContentRoot(repoRoot));
    test_support::EnsureDirectories(test_support::EngineRoot(repoRoot));

    const auto mounts = Resource::DiscoverReadableMountBackends(
        repoRoot, test_support::EngineRoot(repoRoot), test_support::CookedRoot(repoRoot), "Project", "dev");

    ASSERT_EQ(mounts.size(), 2u);
    EXPECT_EQ(mounts[0].sourceKey, "Project");
    EXPECT_EQ(mounts[0].priority, Resource::MountPriority::Source);
    EXPECT_EQ(mounts[0].backend, Resource::MountBackendKind::Directory);

    EXPECT_EQ(mounts[1].sourceKey, "Engine");
    EXPECT_EQ(mounts[1].priority, Resource::MountPriority::Source);
    EXPECT_EQ(mounts[1].backend, Resource::MountBackendKind::Directory);
}
