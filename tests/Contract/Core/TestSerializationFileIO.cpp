#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/Serialization/BuiltinTraits.h"
#include "Core/Serialization/Serialization.h"

using Serialization::JsonBackend;
using Serialization::LoadFromFile;
using Serialization::SaveToFile;

namespace
{
    class SerializationFileIOContractTests : public ::testing::Test
    {
    protected:
        std::filesystem::path TestBaseRoot() const
        {
            return std::filesystem::current_path() / "test-output" / "serialization-file-io";
        }

        std::filesystem::path TestRoot() const
        {
            const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
            return TestBaseRoot() / info->test_suite_name() / info->name();
        }

        std::filesystem::path TestPath(const std::string &relative) const
        {
            return TestRoot() / relative;
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(TestBaseRoot(), ec);
            ec.clear();
            std::filesystem::remove(TestBaseRoot().parent_path(), ec);
        }
    };
} // namespace

TEST_F(SerializationFileIOContractTests, SaveThenLoadIntRoundTrip)
{
    const auto path = TestPath("value.json");
    const JsonBackend backend;
    const int input = 42;
    int output = 0;

    ASSERT_TRUE(SaveToFile(input, path, backend));
    ASSERT_TRUE(LoadFromFile(output, path, backend));
    EXPECT_EQ(output, input);
}

TEST_F(SerializationFileIOContractTests, SaveThenLoadStringRoundTrip)
{
    const auto path = TestPath("value.json");
    const JsonBackend backend;
    const std::string input = "hello";
    std::string output;

    ASSERT_TRUE(SaveToFile(input, path, backend));
    ASSERT_TRUE(LoadFromFile(output, path, backend));
    EXPECT_EQ(output, input);
}

TEST_F(SerializationFileIOContractTests, SaveThenLoadFloatRoundTrip)
{
    const auto path = TestPath("value.json");
    const JsonBackend backend;
    const float input = 3.14f;
    float output = 0.0f;

    ASSERT_TRUE(SaveToFile(input, path, backend));
    ASSERT_TRUE(LoadFromFile(output, path, backend));
    EXPECT_NEAR(output, input, 1e-5f);
}

TEST_F(SerializationFileIOContractTests, SaveToFileCreatesParentDirectories)
{
    const auto path = TestPath("nested/deeper/value.json");
    const JsonBackend backend;

    ASSERT_TRUE(SaveToFile(42, path, backend));
    EXPECT_TRUE(std::filesystem::exists(path.parent_path()));
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(SerializationFileIOContractTests, SaveToFileReturnsFalseWhenTargetPathIsDirectory)
{
    const auto path = TestPath("already-a-directory");
    const JsonBackend backend;

    std::filesystem::create_directories(path);
    EXPECT_FALSE(SaveToFile(42, path, backend));
}

TEST_F(SerializationFileIOContractTests, LoadMissingFileReturnsFalse)
{
    const auto path = TestPath("missing.json");
    const JsonBackend backend;
    int value = 99;

    EXPECT_FALSE(LoadFromFile(value, path, backend));
}

TEST_F(SerializationFileIOContractTests, LoadCorruptedJsonReturnsFalse)
{
    const auto path = TestPath("broken.json");
    const JsonBackend backend;
    int value = 99;

    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "{ bad json";
    }

    EXPECT_FALSE(LoadFromFile(value, path, backend));
}

TEST_F(SerializationFileIOContractTests, LoadParsesButDeserializeFailsReturnsFalse)
{
    const auto path = TestPath("wrong-type.json");
    const JsonBackend backend;
    int value = 99;

    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "\"text\"";
    }

    EXPECT_FALSE(LoadFromFile(value, path, backend));
}

TEST_F(SerializationFileIOContractTests, LoadDoesNotModifyValueOnDeserializeFailure)
{
    const auto path = TestPath("wrong-type.json");
    const JsonBackend backend;
    int value = 99;

    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "\"text\"";
    }

    EXPECT_FALSE(LoadFromFile(value, path, backend));
    EXPECT_EQ(value, 99);
}

TEST_F(SerializationFileIOContractTests, LoadDoesNotModifyValueOnMissingFile)
{
    const auto path = TestPath("missing.json");
    const JsonBackend backend;
    int value = 99;

    EXPECT_FALSE(LoadFromFile(value, path, backend));
    EXPECT_EQ(value, 99);
}

TEST_F(SerializationFileIOContractTests, AutoDetectedBackendWorksForJsonExtension)
{
    const auto path = TestPath("auto.json");
    const std::string input = "hello json";
    std::string output = "sentinel";

    ASSERT_TRUE(SaveToFile(input, path));
    ASSERT_TRUE(LoadFromFile(output, path));
    EXPECT_EQ(output, input);
}
