#pragma once

/// @file DemoSelectorPanel.h
/// @brief ImGui panel that lists registered demos and lets the user switch.
///
/// Returns true from OnImGuiRender() when the user clicks a different demo,
/// signalling LabLayer to tear down the old demo and create the new one.

#include <string>
#include <vector>

class DemoSelectorPanel
{
public:
    /// Render the selectable list. Returns true if the selection changed.
    bool OnImGuiRender(const std::vector<std::string>& demoNames, int& selectedIndex);
};
