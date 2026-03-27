#pragma once

/// @file ConsolePanel.h
/// @brief ImGui debug console with live log display, category filtering,
///        level selector, color-coded entries, and runtime commands.

#include <string>

class ConsolePanel
{
public:
    void OnImGuiRender();

private:
    void DrawMenuBar();
    void DrawLogEntries();
    void DrawCommandInput();
    void ExecuteCommand(const std::string &command);

private:
    int m_CategoryFilter = 0;
    int m_LevelFilter = 0;
    bool m_AutoScroll = true;
    char m_InputBuf[256] = {};
};
