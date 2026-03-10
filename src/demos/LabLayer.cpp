#include "LabLayer.h"

#include <utility>

#include "core/Application.h"
#include "core/Input.h"
#include "DemoBase.h"
#include "DemoRegistry.h"
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

void LabLayer::OnUpdate(float dt)
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
    // demo selector should placed here
    // TODO: Implement demo selector
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

    m_DemosRegistered = true;
}

void LabLayer::SetActiveDemo(Scope<DemoBase> demo, const std::string &name)
{
    if (m_ActiveDemo)
        m_ActiveDemo->OnDetach();

    m_ActiveDemo = std::move(demo);
    m_ActiveDemoName = name;
}