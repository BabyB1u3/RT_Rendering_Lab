#include "DebugPanel.h"

#include <imgui.h>

void DebugPanel::OnImGuiRender()
{
    ImGui::Begin("Debug");

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

    ImGui::End();
}
