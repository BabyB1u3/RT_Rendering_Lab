#include "ConsolePanel.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

#include <imgui.h>
#include <magic_enum.hpp>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include "Core/Diagnostics/Logging/ImGuiConsoleSink.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/Logger.h"
#include "Core/Diagnostics/Logging/LogMacros.h"

namespace
{
std::string TrimWhitespace(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);

    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
    {
        value = value.substr(1, value.size() - 2);
    }

    return value;
}

constexpr auto BuildCategoryMenu()
{
    std::array<const char*, LogCategory::KnownCategories.size() + 1> categories{};
    categories[0] = "All";

    for (size_t i = 0; i < LogCategory::KnownCategories.size(); ++i)
        categories[i + 1] = LogCategory::KnownCategories[i];

    return categories;
}

constexpr auto kCategories = BuildCategoryMenu();

constexpr const char* kLevelNames[] = {"All", "Trace", "Debug", "Info", "Warn", "Error", "Critical"};

bool PassesLevelFilter(spdlog::level::level_enum level, int filterIndex)
{
    switch (filterIndex)
    {
        case 0:
            return true;
        case 1:
            return level == spdlog::level::trace;
        case 2:
            return level == spdlog::level::debug;
        case 3:
            return level >= spdlog::level::info;
        case 4:
            return level >= spdlog::level::warn;
        case 5:
            return level >= spdlog::level::err;
        case 6:
            return level >= spdlog::level::critical;
        default:
            return true;
    }
}

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

const char* LevelTag(spdlog::level::level_enum level)
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

std::optional<spdlog::level::level_enum> ParseLevel(const std::string& str)
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

std::optional<int> FindCategoryMenuIndex(const std::string& category)
{
    for (size_t i = 1; i < kCategories.size(); ++i)
    {
        if (category == kCategories[i])
            return static_cast<int>(i);
    }

    return std::nullopt;
}

bool CanAddressCategoryFromConsole(const std::string& category)
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
    ImGui::Combo("Category", &m_CategoryFilter, kCategories.data(), static_cast<int>(kCategories.size()));
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
    ImGui::BeginChild(
        "LogRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    // Snapshot scroll state BEFORE drawing new content - once new entries push
    // ScrollMaxY higher, the "am I at the bottom?" check would fail.
    const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;

    const auto entries = sink->GetEntries();
    const char* categoryFilter = nullptr;
    if (!m_CommandCategoryFilter.empty())
        categoryFilter = m_CommandCategoryFilter.c_str();
    else if (m_CategoryFilter > 0)
        categoryFilter = kCategories[m_CategoryFilter];

    for (const auto& entry : entries)
    {
        if (!PassesLevelFilter(entry.Level, m_LevelFilter))
            continue;
        if (categoryFilter && entry.Category != categoryFilter)
            continue;

        ImGui::PushStyleColor(ImGuiCol_Text, GetColorForLevel(entry.Level));

        char lineBuf[1024];
        snprintf(lineBuf,
                 sizeof(lineBuf),
                 "%s [%s] %s %s",
                 entry.Timestamp.c_str(),
                 entry.Category.c_str(),
                 LevelTag(entry.Level),
                 entry.Message.c_str());
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
    ImGui::TextDisabled("(log.level, log.filter, log.clear, log.flush, log.json)");
}

void ConsolePanel::ExecuteCommand(const std::string& command)
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
    else if (token == "log.json")
    {
        std::string action;
        stream >> action;

        if (action.empty() || action == "status")
        {
            if (Diagnostics::Logger::IsJsonSinkEnabled())
            {
                LOG_INFO_CAT(LogCategory::ImGui,
                             "JSON log sink is enabled: {}",
                             Diagnostics::Logger::GetJsonSinkPath().string());
            }
            else
            {
                LOG_INFO_CAT(LogCategory::ImGui, "JSON log sink is disabled");
            }
        }
        else if (action == "on")
        {
            const std::string pathStr = TrimWhitespace(
                [&stream]()
                {
                    std::string remainder;
                    std::getline(stream >> std::ws, remainder);
                    return remainder;
                }());

            const std::filesystem::path path =
                pathStr.empty() ? Diagnostics::Logger::GetDefaultJsonLogPath() : std::filesystem::path(pathStr);
            if (path.empty())
            {
                LOG_WARN_CAT(LogCategory::ImGui, "Unable to resolve JSON log path");
                return;
            }

            Diagnostics::Logger::EnableJsonSink(path);
            LOG_INFO_CAT(
                LogCategory::ImGui, "JSON log sink enabled: {}", Diagnostics::Logger::GetJsonSinkPath().string());
        }
        else if (action == "off")
        {
            const auto currentPath = Diagnostics::Logger::GetJsonSinkPath();
            Diagnostics::Logger::DisableJsonSink();

            if (currentPath.empty())
                LOG_INFO_CAT(LogCategory::ImGui, "JSON log sink disabled");
            else
                LOG_INFO_CAT(LogCategory::ImGui, "JSON log sink disabled: {}", currentPath.string());
        }
        else
        {
            LOG_WARN_CAT(LogCategory::ImGui, "Usage: log.json <on|off|status> [path]");
        }
    }
    else
    {
        LOG_WARN_CAT(LogCategory::ImGui, "Unknown command: {}", token);
    }
}
