#pragma once

#include <string>
#include <vector>

class DemoSelectorPanel
{
public:
    // Returns true if the selection changed
    bool OnImGuiRender(const std::vector<std::string> &demoNames, int &selectedIndex);
};
