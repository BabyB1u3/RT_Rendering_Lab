#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace test_support
{
    inline std::filesystem::path WorkingRoot()
    {
        return std::filesystem::current_path() / "test-output";
    }

    inline const ::testing::TestInfo &CurrentTestInfo()
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        return *info;
    }

    inline std::string_view CurrentTestSuiteName()
    {
        return CurrentTestInfo().test_suite_name();
    }

    inline std::string_view CurrentTestName()
    {
        return CurrentTestInfo().name();
    }

    inline std::filesystem::path CategoryRoot(std::string_view category)
    {
        return WorkingRoot() / category;
    }

    inline std::filesystem::path CurrentSuiteRoot(std::string_view category)
    {
        return CategoryRoot(category) / CurrentTestSuiteName();
    }

    inline std::filesystem::path CurrentTestRoot(std::string_view category)
    {
        return CurrentSuiteRoot(category) / CurrentTestName();
    }

    inline std::filesystem::path CurrentTestPath(std::string_view category, std::string_view relativePath)
    {
        return CurrentTestRoot(category) / relativePath;
    }

    inline void RemovePathIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    inline void RemoveDirectoryIfEmpty(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    inline void RemoveTreeIfExists(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    inline void EnsureDirectories(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        ASSERT_FALSE(ec);
    }

    inline void WriteTextFileOrFail(const std::filesystem::path &path, std::string_view contents)
    {
        EnsureDirectories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out << contents;
        ASSERT_TRUE(out.good());
    }

    inline void WriteBinaryFileOrFail(const std::filesystem::path &path, const std::vector<unsigned char> &contents)
    {
        EnsureDirectories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out.write(reinterpret_cast<const char *>(contents.data()), static_cast<std::streamsize>(contents.size()));
        ASSERT_TRUE(out.good());
    }

    inline void ResetCurrentTestRoot(std::string_view category)
    {
        std::error_code ec;
        std::filesystem::remove_all(CurrentTestRoot(category), ec);
        std::filesystem::create_directories(CurrentTestRoot(category), ec);
        ASSERT_FALSE(ec);
    }

    inline void RemoveCurrentTestArtifacts(std::string_view category)
    {
        std::error_code ec;
        std::filesystem::remove_all(CurrentTestRoot(category), ec);
        ec.clear();
        std::filesystem::remove(CurrentSuiteRoot(category), ec);
        ec.clear();
        std::filesystem::remove(CategoryRoot(category), ec);
        ec.clear();
        std::filesystem::remove(WorkingRoot(), ec);
    }
} // namespace test_support
