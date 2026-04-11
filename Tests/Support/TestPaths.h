#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace test_support
{
    inline uint64_t StableComponentHash(std::string_view text)
    {
        uint64_t hash = 14695981039346656037ull;
        for (const unsigned char ch : std::string(text))
        {
            hash ^= ch;
            hash *= 1099511628211ull;
        }

        return hash;
    }

    inline std::string Hex64(uint64_t value)
    {
        static constexpr char kHexDigits[] = "0123456789abcdef";
        std::string text(16, '0');
        for (int i = 15; i >= 0; --i)
        {
            text[static_cast<size_t>(i)] = kHexDigits[value & 0xfu];
            value >>= 4u;
        }

        return text;
    }

    inline std::string ShortPathComponent(std::string_view text)
    {
        constexpr size_t kReadablePrefixLength = 20;
        constexpr size_t kMaxComponentLength = kReadablePrefixLength + 1 + 16;

        if (text.size() <= kMaxComponentLength)
            return std::string(text);

        return std::string(text.substr(0, kReadablePrefixLength)) + "-" + Hex64(StableComponentHash(text));
    }

    inline std::filesystem::path WorkingRoot()
    {
        // Keep test artifact roots short enough for Windows CI path limits.
        return std::filesystem::current_path() / "t";
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
        return CategoryRoot(category) / ShortPathComponent(CurrentTestSuiteName());
    }

    inline std::filesystem::path CurrentTestRoot(std::string_view category)
    {
        return CurrentSuiteRoot(category) / ShortPathComponent(CurrentTestName());
    }

    inline std::filesystem::path CurrentTestPath(std::string_view category, std::string_view relativePath)
    {
        return CurrentTestRoot(category) / relativePath;
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
