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
#include "Demos/02_HelloTriangle/HelloTriangle.h"
#include "Demos/03_HelloTexture/HelloTexture.h"
#include "Demos/04_HelloOffscreen/HelloOffscreen.h"

LabLayer::LabLayer() : Layer("LabLayer") {}

LabLayer::~LabLayer() = default;

void LabLayer::OnAttach()
{
    RegisterBuiltInDemos();

    if (!m_ActiveDemo)
    {
        // Default demo
        m_SelectedDemoIndex = 3;
        LOG_INFO_CAT(LogCategory::k_Demo, "Loading default demo: 04 - Hello Offscreen");
        auto defaultDemo = DemoRegistry::Create("04 - Hello Offscreen");
        RTRLAB_ASSERT_MSG(defaultDemo, "Failed to create default demo: 04 - Hello Offscreen");
        SetActiveDemo(std::move(defaultDemo), "04 - Hello Offscreen");
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
    ApplyPendingDemoSwitch();

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
    // Queue the switch so the old demo survives until this frame's command buffer
    // has finished recording and submission.
    const auto& names = DemoRegistry::GetNames();
    if (m_DemoSelectorPanel.OnImGuiRender(names, m_SelectedDemoIndex))
    {
        const auto& name = names[m_SelectedDemoIndex];
        QueueDemoSwitch(m_SelectedDemoIndex, name);
    }

    m_DebugPanel.OnImGuiRender();
    m_ConsolePanel.OnImGuiRender();

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
    DemoRegistry::Register("02 - Hello Triangle",
                           []()
                           {
                               const auto& window = Application::Get().GetWindow();
                               return CreateScope<HelloTriangle>(window.GetWidth(), window.GetHeight());
                           });
    DemoRegistry::Register("03 - Hello Texture",
                           []()
                           {
                               const auto& window = Application::Get().GetWindow();
                               return CreateScope<HelloTexture>(window.GetWidth(), window.GetHeight());
                           });
    DemoRegistry::Register("04 - Hello Offscreen",
                           []()
                           {
                               const auto& window = Application::Get().GetWindow();
                               return CreateScope<HelloOffscreen>(window.GetWidth(), window.GetHeight());
                           });

    m_DemosRegistered = true;
}

void LabLayer::QueueDemoSwitch(int demoIndex, const std::string& name)
{
    m_PendingDemoIndex = demoIndex;
    m_PendingDemoName = name;
    m_HasPendingDemoSwitch = true;
}

void LabLayer::ApplyPendingDemoSwitch()
{
    if (!m_HasPendingDemoSwitch)
        return;

    const int demoIndex = m_PendingDemoIndex;
    const std::string name = m_PendingDemoName;
    m_HasPendingDemoSwitch = false;
    m_PendingDemoName.clear();

    auto demo = DemoRegistry::Create(name);
    if (!demo)
    {
        LOG_ERROR_CAT(LogCategory::k_Demo, "Failed to create demo: {}", name);
        return;
    }

    m_SelectedDemoIndex = demoIndex;
    SetActiveDemo(std::move(demo), name);
    if (m_ActiveDemo)
        m_ActiveDemo->OnAttach();
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
