#include <gtest/gtest.h>

#include "Core/Diagnostics/Logging/Logger.h"

#include "DiagnosticsTestSupport.h"

TEST(LoggerConsoleSinkIntegrationTests, ConsoleSinkIsRegisteredByLogger)
{
    const auto logPath = DiagnosticsTestSupport::TestPath("diagnostics-console-sink.log");
    Diagnostics::Logger::Init(logPath);

    const auto sink = Diagnostics::Logger::GetConsoleSink();
    EXPECT_NE(sink, nullptr);

    Diagnostics::Logger::Shutdown();
    DiagnosticsTestSupport::RemoveCurrentTestArtifacts();
}
