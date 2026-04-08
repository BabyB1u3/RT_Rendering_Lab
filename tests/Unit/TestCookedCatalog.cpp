#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "Core/Resource/CookedCatalog.h"
#include "Core/Resource/SourceCatalog.h"

namespace
{
    class CookedCatalogTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = std::filesystem::current_path() / "test-output" / "cooked-catalog" /
                         ::testing::UnitTest::GetInstance()->current_test_info()->name();

            std::error_code ec;
            std::filesystem::remove_all(m_TestRoot, ec);
            std::filesystem::create_directories(m_TestRoot);
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::current_path() / "test-output" / "cooked-catalog", ec);
            ec.clear();
            std::filesystem::remove(std::filesystem::current_path() / "test-output", ec);
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

        void WriteTextFile(const std::filesystem::path &path, std::string_view contents) const
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream out(path, std::ios::binary);
            ASSERT_TRUE(out.is_open());
            out << contents;
            ASSERT_TRUE(out.good());
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace

TEST_F(CookedCatalogTests, CookRepositoryCatalogsCopiesArtifactsAndWritesCookedCatalog)
{
    const auto repoRoot = TestRoot() / "Repo";
    WriteTextFile(repoRoot / "Content" / "textures" / "Grassy_Square.jpg", "jpg");

    std::string errorMessage;
    ASSERT_TRUE(Resource::IndexRepositorySourceCatalogs(repoRoot, "Content", &errorMessage)) << errorMessage;

    const auto cookedRoot = repoRoot / "Saved" / "Cache" / "Cooked";
    ASSERT_TRUE(Resource::CookRepositoryCatalogs(repoRoot, cookedRoot, "Content", &errorMessage)) << errorMessage;

    const auto cookedCatalogPath = cookedRoot / "Project" / ".rtr" / "catalog.json";
    const auto cookedArtifactPath = cookedRoot / "Project" / "Textures" / "Grassy_Square.ktx2";

    EXPECT_TRUE(std::filesystem::exists(cookedCatalogPath));
    EXPECT_TRUE(std::filesystem::exists(cookedArtifactPath));

    std::ifstream cookedCatalog(cookedCatalogPath);
    ASSERT_TRUE(cookedCatalog.is_open());
    const std::string cookedContents((std::istreambuf_iterator<char>(cookedCatalog)), std::istreambuf_iterator<char>());

    EXPECT_NE(cookedContents.find("/Project/Textures/Grassy_Square"), std::string::npos);
    EXPECT_NE(cookedContents.find("\"profileTag\": \"cooked\""), std::string::npos);
    EXPECT_NE(cookedContents.find("\"relativePath\": \"Textures/Grassy_Square.ktx2\""), std::string::npos);
    EXPECT_NE(cookedContents.find("\"format\": \"ktx2\""), std::string::npos);

    std::ifstream cookedArtifact(cookedArtifactPath, std::ios::binary);
    ASSERT_TRUE(cookedArtifact.is_open());
    const std::string artifactContents((std::istreambuf_iterator<char>(cookedArtifact)), std::istreambuf_iterator<char>());
    EXPECT_EQ(artifactContents, "jpg");
}
