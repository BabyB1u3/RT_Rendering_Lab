#include "ConsolePanel.h"

#include <array>
#include <cstddef>
#include <optional>
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
    constexpr auto BuildCategoryMenu()
    {
        std::array<const char *, LogCategory::KnownCategories.size() + 1> categories{};
        categories[0] = "All";

        for (size_t i = 0; i < LogCategory::KnownCategories.size(); ++i)
            categories[i + 1] = LogCategory::KnownCategories[i];

        return categories;
    }

    constexpr auto kCategories = BuildCategoryMenu();

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

    std::optional<int> FindCategoryMenuIndex(const std::string &category)
    {
        for (size_t i = 1; i < kCategories.size(); ++i)
        {
            if (category == kCategories[i])
                return static_cast<int>(i);
        }

        return std::nullopt;
    }

    bool CanAddressCategoryFromConsole(const std::string &category)
    {
        return LogCategory::IsKnownCategory(category) || Diagnostics::Logger::HasLogger(category.c_str());
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
    const int previousCategoryFilter = m_CategoryFilter;
    ImGui::Combo("Category", &m_CategoryFilter, kCategories.data(),
                 static_cast<int>(kCategories.size()));
    if (m_CategoryFilter != previousCategoryFilter)
        m_CommandCategoryFilter.clear();

    if (!m_CommandCategoryFilter.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Cmd: %s", m_CommandCategoryFilter.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Cmd Filter"))
        {
            m_CommandCategoryFilter.clear();
            m_CategoryFilter = 0;
        }
    }

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

    // Snapshot scroll state BEFORE drawing new content — once new entries push
    // ScrollMaxY higher, the "am I at the bottom?" check would fail.
    const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    const auto entries = sink->GetEntries();
    const char *categoryFilter = nullptr;
    if (!m_CommandCategoryFilter.empty())
        categoryFilter = m_CommandCategoryFilter.c_str();
    else if (m_CategoryFilter > 0)
        categoryFilter = kCategories[m_CategoryFilter];
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

    if (m_AutoScroll && wasAtBottom)
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
        LOG_INFO_CAT(LogCategory::ImGui, "Flushed all sinks");
    }
    else if (token == "log.level")
    {
        std::string target, levelStr;
        stream >> target >> levelStr;

        if (target.empty() || levelStr.empty())
        {
            LOG_WARN_CAT(LogCategory::ImGui, "Usage: log.level <category|*> <level>");
            return;
        }

        auto level = ParseLevel(levelStr);
        if (!level)
        {
            LOG_WARN_CAT(LogCategory::ImGui, "Unknown log level: {}", levelStr);
            return;
        }

        if (target == "*")
        {
            Diagnostics::Logger::SetGlobalLevel(*level);
            LOG_INFO_CAT(LogCategory::ImGui, "Global log level set to {}", magic_enum::enum_name(*level));
        }
        else if (CanAddressCategoryFromConsole(target))
        {
            Diagnostics::Logger::SetLevel(target.c_str(), *level);
            LOG_INFO_CAT(LogCategory::ImGui, "Log level for '{}' set to {}", target, magic_enum::enum_name(*level));
        }
        else
        {
            LOG_WARN_CAT(LogCategory::ImGui, "Unknown category: '{}'. No logger registered with that name.", target);
        }
    }
    else if (token == "log.filter")
    {
        std::string target;
        stream >> target;

        if (target.empty() || target == "*")
        {
            m_CategoryFilter = 0;
            m_CommandCategoryFilter.clear();
            LOG_INFO_CAT(LogCategory::ImGui, "Category filter cleared");
        }
        else if (CanAddressCategoryFromConsole(target))
        {
            if (const auto menuIndex = FindCategoryMenuIndex(target))
            {
                m_CategoryFilter = *menuIndex;
                m_CommandCategoryFilter.clear();
            }
            else
            {
                m_CategoryFilter = 0;
                m_CommandCategoryFilter = target;
            }

            LOG_INFO_CAT(LogCategory::ImGui, "Category filter set to '{}'", target);
        }
        else
        {
            LOG_WARN_CAT(LogCategory::ImGui, "Unknown category: {}", target);
        }
    }
    else
    {
        LOG_WARN_CAT(LogCategory::ImGui, "Unknown command: {}", token);
    }
}
