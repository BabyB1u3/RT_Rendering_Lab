#pragma once

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ResourceTestSupport.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#endif

namespace test_support
{
    inline std::filesystem::path GetExecutableDirectoryForTest()
    {
        std::filesystem::path exePath;

#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
            exePath = std::filesystem::path(buf);
#elif defined(__APPLE__)
        char buf[PATH_MAX];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0)
            exePath = std::filesystem::canonical(buf);
#elif defined(__linux__)
        char buf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            exePath = std::filesystem::path(buf);
        }
#endif

        if (exePath.empty())
            return {};

        return exePath.parent_path();
    }

    class ScopedEnvVar
    {
    public:
        ScopedEnvVar(const char *name, std::string_view value)
            : m_Name(name)
        {
            if (const char *existing = std::getenv(name))
            {
                m_PreviousValue = existing;
                m_HadPreviousValue = true;
            }

            Set(value);
        }

        ~ScopedEnvVar()
        {
            if (m_HadPreviousValue)
                Set(*m_PreviousValue);
            else
                Clear();
        }

    private:
        void Set(std::string_view value)
        {
#if defined(_WIN32)
            _putenv_s(m_Name.c_str(), std::string(value).c_str());
#else
            setenv(m_Name.c_str(), std::string(value).c_str(), 1);
#endif
        }

        void Clear()
        {
#if defined(_WIN32)
            _putenv_s(m_Name.c_str(), "");
#else
            unsetenv(m_Name.c_str());
#endif
        }

        std::string m_Name;
        std::optional<std::string> m_PreviousValue;
        bool m_HadPreviousValue = false;
    };

    class RootDiscoveryTestsBase : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_TestRoot = CurrentTestRoot("root-discovery");
            ResetCurrentTestRoot("root-discovery");
        }

        void TearDown() override
        {
            RemoveCurrentTestArtifacts("root-discovery");
        }

        std::filesystem::path TestRoot() const
        {
            return m_TestRoot;
        }

    private:
        std::filesystem::path m_TestRoot;
    };
} // namespace test_support
