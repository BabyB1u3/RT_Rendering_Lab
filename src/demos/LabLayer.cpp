#include "LabLayer.h"

#include <utility>

#include "core/Application.h"
#include "core/Events.h"
#include "core/Input.h"
#include "core/Logger.h"
#include "DemoBase.h"
#include "DemoRegistry.h"
#include "demos/MaterialPlayground/MaterialPlayground.h"
#include "demos/ShadowMapping/ShadowMapping.h"

LabLayer::LabLayer()
    : Layer("LabLayer")
{
}

void LabLayer::OnAttach()
{
    Input::Initialize(Application::Get().GetWindow().GetNativeHandle());

    RegisterBuiltInDemos();

    if (!m_ActiveDemo)
    {
        // Default demo
        LOG_INFO("Loading default demo: Shadow Mapping");
        SetActiveDemo(DemoRegistry::Create("Shadow Mapping"), "Shadow Mapping");
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
    const auto &names = DemoRegistry::GetNames();
    if (m_DemoSelectorPanel.OnImGuiRender(names, m_SelectedDemoIndex))
    {
        const auto &name = names[m_SelectedDemoIndex];
        SetActiveDemo(DemoRegistry::Create(name), name);
        if (m_ActiveDemo)
            m_ActiveDemo->OnAttach();
    }

    m_DebugPanel.OnImGuiRender();

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

    DemoRegistry::Register("Shadow Mapping", []()
                           {
        const auto& window = Application::Get().GetWindow();
        return CreateScope<ShadowMapping>(window.GetWidth(), window.GetHeight()); });

    DemoRegistry::Register("Material Playground", []()
                           {
        const auto& window = Application::Get().GetWindow();
        return CreateScope<MaterialPlayground>(window.GetWidth(), window.GetHeight()); });

    m_DemosRegistered = true;
}

void LabLayer::SetActiveDemo(Scope<DemoBase> demo, const std::string &name)
{
    if (m_ActiveDemo)
        m_ActiveDemo->OnDetach();

    m_ActiveDemo = std::move(demo);
    m_ActiveDemoName = name;
    LOG_INFO("Active demo: {}", m_ActiveDemoName);

    Application::Get().GetEventBus().Publish(
        DemoSwitchedEvent{ m_SelectedDemoIndex, m_ActiveDemoName.c_str() });
}