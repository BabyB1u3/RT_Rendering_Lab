#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <spdlog/logger.h>

#include "Core/Diagnostics/Logging/ImGuiConsoleSink.h"
#include "Core/Diagnostics/Logging/Logger.h"

#include "DiagnosticsTestSupport.h"

class ImGuiConsoleSinkTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_Sink = std::make_shared<Diagnostics::ImGuiConsoleSink>();
    }

    void LogMessage(const std::string &category, spdlog::level::level_enum level, const std::string &msg)
    {
        auto logger = std::make_shared<spdlog::logger>(category, m_Sink);
        logger->set_level(spdlog::level::trace);
        logger->log(level, msg);
    }

    std::shared_ptr<Diagnostics::ImGuiConsoleSink> m_Sink;
};

TEST_F(ImGuiConsoleSinkTests, EntriesAreEmptyByDefault)
{
    EXPECT_TRUE(m_Sink->GetEntries().empty());
}

TEST_F(ImGuiConsoleSinkTests, SinkCapturesLogMessages)
{
    LogMessage("Core", spdlog::level::info, "hello world");

    const auto entries = m_Sink->GetEntries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].Category, "Core");
    EXPECT_EQ(entries[0].Level, spdlog::level::info);
    EXPECT_EQ(entries[0].Message, "hello world");
    EXPECT_FALSE(entries[0].Timestamp.empty());
}

TEST_F(ImGuiConsoleSinkTests, ClearRemovesAllEntries)
{
    LogMessage("Core", spdlog::level::info, "msg1");
    LogMessage("Core", spdlog::level::warn, "msg2");
    ASSERT_EQ(m_Sink->GetEntries().size(), 2u);

    m_Sink->Clear();
    EXPECT_TRUE(m_Sink->GetEntries().empty());
}

TEST_F(ImGuiConsoleSinkTests, RingBufferEvictsOldestWhenFull)
{
    for (int i = 0; i < 1030; ++i)
        LogMessage("Core", spdlog::level::trace, "msg" + std::to_string(i));

    const auto entries = m_Sink->GetEntries();
    EXPECT_EQ(entries.size(), 1024u);
    EXPECT_EQ(entries.front().Message, "msg6");
    EXPECT_EQ(entries.back().Message, "msg1029");
}

TEST_F(ImGuiConsoleSinkTests, MultipleCategoriesAreCaptured)
{
    LogMessage("Shader", spdlog::level::err, "compile failed");
    LogMessage("Window", spdlog::level::info, "created");

    const auto entries = m_Sink->GetEntries();
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].Category, "Shader");
    EXPECT_EQ(entries[1].Category, "Window");
}

TEST_F(ImGuiConsoleSinkTests, ConsoleSinkIsRegisteredByLogger)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-contract.log");
    Diagnostics::Logger::Init(logPath);

    const auto sink = Diagnostics::Logger::GetConsoleSink();
    EXPECT_NE(sink, nullptr);

    Diagnostics::Logger::Shutdown();
    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}
