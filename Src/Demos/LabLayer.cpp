#include "LabLayer.h"

#include <utility>

#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/App/Application.h"
#include "Core/Event/Events.h"
#include "Core/Input/Input.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "DemoBase.h"
#include "DemoRegistry.h"
#include "Demos/01_HelloWindow/HelloWindow.h"

LabLayer::LabLayer() : Layer("LabLayer") {}

LabLayer::~LabLayer() = default;

void LabLayer::OnAttach()
{
    RegisterBuiltInDemos();

    if (!m_ActiveDemo)
    {
        // Default demo
        LOG_INFO_CAT(LogCategory::k_Demo, "Loading default demo: 01 - Hello Window");
        auto defaultDemo = DemoRegistry::Create("01 - Hello Window");
        RTRLAB_ASSERT_MSG(defaultDemo, "Failed to create default demo: 01 - Hello Window");
        SetActiveDemo(std::move(defaultDemo), "01 - Hello Window");
    }

    if (m_ActiveDemo)
        m_ActiveDemo->OnAttach();
}

void LabLayer::OnDetach()
{
    if (m_ActiveDemo)
        m_ActiveDemo->OnDetach();
}

void LabLayer::OnUpdate(double dt)
{
    if (m_ActiveDemo)
        m_ActiveDemo->OnUpdate(dt);
}

void LabLayer::OnRender()
{
    if (m_ActiveDemo)
        m_ActiveDemo->OnRender();
}

void LabLayer::OnImGuiRender()
{
    // When DemoSelectorPanel returns true, the user picked a different demo.
    // SetActiveDemo detaches the old demo; we then attach the new one.
    // const auto &names = DemoRegistry::GetNames();
    // if (m_DemoSelectorPanel.OnImGuiRender(names, m_SelectedDemoIndex))
    // {
    //     const auto &name = names[m_SelectedDemoIndex];
    //     auto demo = DemoRegistry::Create(name);
    //     if (!demo)
    //     {
    //         LOG_ERROR_CAT(LogCategory::k_Demo, "Failed to create demo: {}", name);
    //         return;
    //     }

    //     SetActiveDemo(std::move(demo), name);
    //     if (m_ActiveDemo)
    //         m_ActiveDemo->OnAttach();
    // }

    // m_DebugPanel.OnImGuiRender();
    // m_ConsolePanel.OnImGuiRender();

    if (m_ActiveDemo)
        m_ActiveDemo->OnImGuiRender();
}

void LabLayer::OnResize(uint32_t width, uint32_t height)
{
    if (m_ActiveDemo)
        m_ActiveDemo->OnResize(width, height);
}

void LabLayer::RegisterBuiltInDemos()
{
    if (m_DemosRegistered)
        return;
    // --- Demos ---
    DemoRegistry::Register("01 - Hello Window",
                           []()
                           {
                               const auto& window = Application::Get().GetWindow();
                               return CreateScope<HelloWindow>(window.GetWidth(), window.GetHeight());
                           });

    m_DemosRegistered = true;
}

void LabLayer::SetActiveDemo(Scope<DemoBase> demo, const std::string& name)
{
    if (!demo)
    {
        LOG_ERROR_CAT(LogCategory::k_Demo, "SetActiveDemo called with null demo: {}", name);
        return;
    }

    if (m_ActiveDemo)
        m_ActiveDemo->OnDetach();

    m_ActiveDemo = std::move(demo);
    m_ActiveDemoName = name;
    LOG_INFO_CAT(LogCategory::k_Demo, "Active demo: {}", m_ActiveDemoName);

    Application::Get().GetEventBus().Publish(DemoSwitchedEvent{m_SelectedDemoIndex, m_ActiveDemoName.c_str()});
}
