#pragma once

/// @file LabLayer.h
/// @brief Main application Layer that hosts the demo framework.
///
/// LabLayer is the single Layer pushed onto the Application's LayerStack.
/// It owns the active demo (a DemoBase subclass), a DemoSelectorPanel for
/// hot-switching demos at runtime, and a DebugPanel for FPS display.
///
/// Lifecycle flow per frame:
///   OnUpdate(dt) → OnRender() → [ImGui] OnImGuiRender()
/// All three are forwarded to the currently active demo.

#include <memory>
#include <string>

#include "Core/App/Layer.h"
#include "Core/Base.h"
#include "GUI/Panels/ConsolePanel.h"
#include "GUI/Panels/DebugPanel.h"
#include "GUI/Panels/DemoSelectorPanel.h"

class DemoBase;

class LabLayer : public Layer
{
public:
    LabLayer();
    ~LabLayer() override;

    void OnAttach() override;
    void OnDetach() override;

    void OnUpdate(double dt) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnResize(uint32_t width, uint32_t height) override;

private:
    void RegisterBuiltInDemos();
    void SetActiveDemo(Scope<DemoBase> demo, const std::string &name);

private:
    bool m_DemosRegistered = false;

    int m_SelectedDemoIndex = 0;
    std::string m_ActiveDemoName;
    Scope<DemoBase> m_ActiveDemo;

    ConsolePanel m_ConsolePanel;
    DebugPanel m_DebugPanel;
    DemoSelectorPanel m_DemoSelectorPanel;
};
