#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/Serialization/BuiltinTraits.h"
#include "Core/Serialization/Serialization.h"
#include "Core/Resource/FileSystem.h"

using Serialization::JsonBackend;
using Serialization::LoadFromFile;
using Serialization::LoadFromConfigPath;
using Serialization::LoadFromVirtualPath;
using Serialization::SaveToFile;
using Serialization::SaveToConfigPath;
using Serialization::SaveToVirtualPath;

namespace
{
    struct MaterialReferenceRecord
    {
        Resource::AssetPath albedo;
    };

    void Serialize(Serialization::PropertyTree &tree, const MaterialReferenceRecord &value)
    {
        tree = Serialization::PropertyTree::Object{};
        Serialization::PropertyTree albedoTree;
        Serialize(albedoTree, value.albedo);
        tree["albedo"] = std::move(albedoTree);
    }

    bool Deserialize(const Serialization::PropertyTree &tree, MaterialReferenceRecord &value)
    {
        if (!tree.IsObject() || !tree.Contains("albedo"))
            return false;
        return Deserialize(tree["albedo"], value.albedo);
    }

    class SerializationFileIOContractTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            FileSystem::Init();
        }

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

        std::string VirtualSavedPath(const std::string &relative) const
        {
            return "/Saved/SerializationContract/" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) + "/" + relative;
        }

        std::filesystem::path ResolveSavedVirtualPath(const std::string &relative) const
        {
            const auto path = FileSystem::ResolveWritePath(VirtualSavedPath(relative));
            return path.value_or(std::filesystem::path{});
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(TestBaseRoot(), ec);
            ec.clear();
            std::filesystem::remove(TestBaseRoot().parent_path(), ec);

            const auto savedRoot = FileSystem::ResolveWritePath("/Saved/SerializationContract");
            if (savedRoot.has_value())
            {
                ec.clear();
                std::filesystem::remove_all(*savedRoot, ec);
            }
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

TEST_F(SerializationFileIOContractTests, SaveThenLoadStringRoundTripThroughVirtualSavedPath)
{
    constexpr std::string_view kRelative = "value.json";
    const JsonBackend backend;
    const std::string input = "hello virtual";
    std::string output = "sentinel";

    ASSERT_TRUE(SaveToVirtualPath(input, VirtualSavedPath(std::string(kRelative)), backend));
    ASSERT_TRUE(LoadFromVirtualPath(output, VirtualSavedPath(std::string(kRelative)), backend));
    EXPECT_EQ(output, input);
    EXPECT_TRUE(std::filesystem::exists(ResolveSavedVirtualPath(std::string(kRelative))));
}

TEST_F(SerializationFileIOContractTests, AutoDetectedBackendWorksForVirtualSavedPath)
{
    constexpr std::string_view kRelative = "auto.json";
    const std::string input = "hello logical";
    std::string output = "sentinel";

    ASSERT_TRUE(SaveToVirtualPath(input, VirtualSavedPath(std::string(kRelative))));
    ASSERT_TRUE(LoadFromVirtualPath(output, VirtualSavedPath(std::string(kRelative))));
    EXPECT_EQ(output, input);
}

TEST_F(SerializationFileIOContractTests, SaveToVirtualPathRejectsReadOnlyDomains)
{
    EXPECT_FALSE(SaveToVirtualPath(42, "/Project/Config/test-contract/serialization.json"));
}

TEST_F(SerializationFileIOContractTests, LoadFromVirtualPathDoesNotModifyValueOnMissingFile)
{
    int value = 99;

    EXPECT_FALSE(LoadFromVirtualPath(value, "/Saved/SerializationContract/missing.json"));
    EXPECT_EQ(value, 99);
}

TEST_F(SerializationFileIOContractTests, LoadSerializedAssetReferenceFromVirtualPath)
{
    const auto assetPath = Resource::AssetPath::TryCreate("/Project/Textures/Grassy_Square");
    ASSERT_TRUE(assetPath.has_value());

    const MaterialReferenceRecord input{*assetPath};
    MaterialReferenceRecord output{*assetPath};

    ASSERT_TRUE(SaveToVirtualPath(input, VirtualSavedPath("material-ref.json")));
    ASSERT_TRUE(LoadFromVirtualPath(output, VirtualSavedPath("material-ref.json")));
    EXPECT_EQ(output.albedo, *assetPath);
}

TEST_F(SerializationFileIOContractTests, LoadSerializedAssetReferenceRejectsAbsoluteFilesystemPath)
{
    const auto path = ResolveSavedVirtualPath("invalid-material-ref.json");
    ASSERT_FALSE(path.empty());
    std::filesystem::create_directories(path.parent_path());

    {
        std::ofstream out(path);
        ASSERT_TRUE(out.is_open());
        out << "{\n"
            << "  \"albedo\": \"C:/Users/name/dev/RTRLab/Content/textures/Grassy_Square.jpg\"\n"
            << "}\n";
    }

    const auto original = Resource::AssetPath::TryCreate("/Project/Textures/Original");
    ASSERT_TRUE(original.has_value());
    MaterialReferenceRecord value{*original};

    EXPECT_FALSE(LoadFromVirtualPath(value, VirtualSavedPath("invalid-material-ref.json")));
    EXPECT_EQ(value.albedo, *original);
}

TEST_F(SerializationFileIOContractTests, SaveToConfigPathWritesIntoSavedConfigNamespace)
{
    constexpr std::string_view kRelative = "serialization-contract/ConfigValue.json";
    const std::string input = "config saved";
    std::string output = "sentinel";

    ASSERT_TRUE(SaveToConfigPath(input, kRelative));
    ASSERT_TRUE(LoadFromVirtualPath(output, "/Saved/Config/serialization-contract/ConfigValue.json"));
    EXPECT_EQ(output, input);
}

TEST_F(SerializationFileIOContractTests, LoadFromConfigPathFallsBackToEngineDefaults)
{
    const auto projectPath = FileSystem::GetAssetPath("Config") / "input/DefaultBindings.json";
    const auto savedPath = FileSystem::GetSavedConfigPath("input/DefaultBindings.json");

    std::error_code ec;
    std::filesystem::remove(projectPath, ec);
    ec.clear();
    std::filesystem::remove(savedPath, ec);

    std::string value = "sentinel";
    ASSERT_TRUE(LoadFromConfigPath(value, "input/DefaultBindings.json"));
    EXPECT_NE(value.find("engine-default"), std::string::npos);

    ec.clear();
    std::filesystem::remove(savedPath, ec);
    ec.clear();
    std::filesystem::remove(savedPath.parent_path(), ec);
}
