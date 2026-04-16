#pragma once

/// @file ConsolePanel.h
/// @brief ImGui debug console with live log display, category filtering,
///        level selector, color-coded entries, and runtime commands.

#include <string>

class ConsolePanel
{
public:
    void OnImGuiRender();
    void ExecuteCommand(const std::string& command);

    const std::string& GetCommandCategoryFilter() const { return m_CommandCategoryFilter; }
    int GetCategoryFilterIndex() const { return m_CategoryFilter; }

private:
    void DrawMenuBar();
    void DrawLogEntries();
    void DrawCommandInput();

private:
    int m_CategoryFilter = 0;
    int m_LevelFilter = 0;
    bool m_AutoScroll = true;
    std::string m_CommandCategoryFilter;
    char m_InputBuf[256] = {};
};
