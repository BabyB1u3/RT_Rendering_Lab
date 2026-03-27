#include "ConsolePanel.h"

#include <array>
#include <sstream>
#include <string>

#include <imgui.h>
#include <magic_enum.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "core/diagnostics/ImGuiConsoleSink.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/Logger.h"
#include "core/diagnostics/LogMacros.h"

namespace
{
    constexpr std::array kCategories = {
        "All",
        LogCategory::Core,
        LogCategory::Graphics,
        LogCategory::Renderer,
        LogCategory::Shader,
        LogCategory::Input,
        LogCategory::FileSystem,
        LogCategory::Window,
        LogCategory::ImGui,
        LogCategory::Demo,
        LogCategory::Assert,
        LogCategory::Ensure,
        LogCategory::Error,
        LogCategory::Crash,
    };

    constexpr const char *kLevelNames[] = {"All", "Trace", "Info", "Warn", "Error", "Critical"};

    constexpr spdlog::level::level_enum kLevelValues[] = {
        spdlog::level::trace,
        spdlog::level::trace,
        spdlog::level::info,
        spdlog::level::warn,
        spdlog::level::err,
        spdlog::level::critical,
    };

    ImVec4 GetColorForLevel(spdlog::level::level_enum level)
    {
        switch (level)
        {
        case spdlog::level::trace:
            return {0.6f, 0.6f, 0.6f, 1.0f};
        case spdlog::level::debug:
            return {0.6f, 0.8f, 1.0f, 1.0f};
        case spdlog::level::info:
            return {1.0f, 1.0f, 1.0f, 1.0f};
        case spdlog::level::warn:
            return {1.0f, 1.0f, 0.4f, 1.0f};
        case spdlog::level::err:
            return {1.0f, 0.4f, 0.4f, 1.0f};
        case spdlog::level::critical:
            return {1.0f, 0.2f, 0.8f, 1.0f};
        default:
            return {1.0f, 1.0f, 1.0f, 1.0f};
        }
    }

    const char *LevelTag(spdlog::level::level_enum level)
    {
        switch (level)
        {
        case spdlog::level::trace:
            return "[trace]";
        case spdlog::level::debug:
            return "[debug]";
        case spdlog::level::info:
            return "[info] ";
        case spdlog::level::warn:
            return "[warn] ";
        case spdlog::level::err:
            return "[error]";
        case spdlog::level::critical:
            return "[crit] ";
        default:
            return "[???]  ";
        }
    }

    std::optional<spdlog::level::level_enum> ParseLevel(const std::string &str)
    {
        auto result = magic_enum::enum_cast<spdlog::level::level_enum>(str, magic_enum::case_insensitive);
        if (result.has_value())
            return result.value();
        if (str == "error")
            return spdlog::level::err;
        if (str == "warning")
            return spdlog::level::warn;
        return std::nullopt;
    }
} // namespace

void ConsolePanel::OnImGuiRender()
{
    ImGui::Begin("Console");

    DrawMenuBar();
    DrawLogEntries();
    DrawCommandInput();

    ImGui::End();
}

void ConsolePanel::DrawMenuBar()
{
    if (ImGui::Button("Clear"))
    {
        auto sink = Diagnostics::Logger::GetConsoleSink();
        if (sink)
            sink->Clear();
    }

    ImGui::SameLine();
    if (ImGui::Button("Flush"))
        Diagnostics::Logger::Flush();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_AutoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("Category", &m_CategoryFilter, kCategories.data(),
                 static_cast<int>(kCategories.size()));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Level", &m_LevelFilter, kLevelNames, IM_ARRAYSIZE(kLevelNames));

    ImGui::Separator();
}

void ConsolePanel::DrawLogEntries()
{
    auto sink = Diagnostics::Logger::GetConsoleSink();
    if (!sink)
    {
        ImGui::TextDisabled("Console sink not initialized");
        return;
    }

    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("LogRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    const auto entries = sink->GetEntries();
    const char *categoryFilter = (m_CategoryFilter > 0) ? kCategories[m_CategoryFilter] : nullptr;
    const auto levelFloor = kLevelValues[m_LevelFilter];

    for (const auto &entry : entries)
    {
        if (entry.Level < levelFloor)
            continue;
        if (categoryFilter && entry.Category != categoryFilter)
            continue;

        ImGui::PushStyleColor(ImGuiCol_Text, GetColorForLevel(entry.Level));

        char lineBuf[1024];
        snprintf(lineBuf, sizeof(lineBuf), "%s [%s] %s %s",
                 entry.Timestamp.c_str(), entry.Category.c_str(),
                 LevelTag(entry.Level), entry.Message.c_str());
        ImGui::TextUnformatted(lineBuf);

        ImGui::PopStyleColor();
    }

    if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

void ConsolePanel::DrawCommandInput()
{
    ImGui::Separator();

    bool reclaim = false;
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (ImGui::InputText("##cmd", m_InputBuf, sizeof(m_InputBuf), flags))
    {
        std::string cmd(m_InputBuf);
        if (!cmd.empty())
        {
            ExecuteCommand(cmd);
            m_InputBuf[0] = '\0';
        }
        reclaim = true;
    }

    ImGui::SetItemDefaultFocus();
    if (reclaim)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::SameLine();
    ImGui::TextDisabled("(log.level, log.filter, log.clear, log.flush)");
}

void ConsolePanel::ExecuteCommand(const std::string &command)
{
    std::istringstream stream(command);
    std::string token;
    stream >> token;

    if (token == "log.clear")
    {
        auto sink = Diagnostics::Logger::GetConsoleSink();
        if (sink)
            sink->Clear();
    }
    else if (token == "log.flush")
    {
        Diagnostics::Logger::Flush();
        LOG_INFO("Flushed all sinks");
    }
    else if (token == "log.level")
    {
        std::string target, levelStr;
        stream >> target >> levelStr;

        if (target.empty() || levelStr.empty())
        {
            LOG_WARN("Usage: log.level <category|*> <level>");
            return;
        }

        auto level = ParseLevel(levelStr);
        if (!level)
        {
            LOG_WARN("Unknown log level: {}", levelStr);
            return;
        }

        if (target == "*")
        {
            Diagnostics::Logger::SetGlobalLevel(*level);
            LOG_INFO("Global log level set to {}", magic_enum::enum_name(*level));
        }
        else
        {
            Diagnostics::Logger::SetLevel(target.c_str(), *level);
            LOG_INFO("Log level for '{}' set to {}", target, magic_enum::enum_name(*level));
        }
    }
    else if (token == "log.filter")
    {
        std::string target;
        stream >> target;

        if (target.empty() || target == "*")
        {
            m_CategoryFilter = 0;
            LOG_INFO("Category filter cleared");
        }
        else
        {
            for (size_t i = 1; i < kCategories.size(); ++i)
            {
                if (target == kCategories[i])
                {
                    m_CategoryFilter = static_cast<int>(i);
                    LOG_INFO("Category filter set to '{}'", target);
                    return;
                }
            }
            LOG_WARN("Unknown category: {}", target);
        }
    }
    else
    {
        LOG_WARN("Unknown command: {}", token);
    }
}
