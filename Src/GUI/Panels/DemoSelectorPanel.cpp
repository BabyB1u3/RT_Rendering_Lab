#include "DemoSelectorPanel.h"

#include <imgui.h>

bool DemoSelectorPanel::OnImGuiRender(const std::vector<std::string>& demoNames, int& selectedIndex)
{
    bool hasSelectionChanged = false;

    ImGui::Begin("Demo Selector");

    for (int i = 0; i < static_cast<int>(demoNames.size()); ++i)
    {
        bool isSelected = (i == selectedIndex);
        if (ImGui::Selectable(demoNames[i].c_str(), isSelected))
        {
            if (i != selectedIndex)
            {
                selectedIndex = i;
                hasSelectionChanged = true;
            }
        }
    }

    ImGui::End();

    return hasSelectionChanged;
}
