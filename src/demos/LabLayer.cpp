#include "LabLayer.h"

#include <utility>

#include "core/diagnostics/Assert.h"
#include "core/diagnostics/LogCategories.h"
#include "core/app/Application.h"
#include "core/event/Events.h"
#include "core/input/Input.h"
#include "core/diagnostics/LogMacros.h"
#include "DemoBase.h"
#include "DemoRegistry.h"
#include "demos/showcase/MaterialPlayground/MaterialPlayground.h"
#include "demos/showcase/ShadowMapping/ShadowMapping.h"
#include "demos/tutorial/01_ClearScreen/ClearScreen.h"
#include "demos/tutorial/02_Triangle/Triangle.h"
#include "demos/tutorial/03_TexturedQuad/TexturedQuad.h"
#include "demos/tutorial/04_TransformedCube/TransformedCube.h"
#include "demos/tutorial/05_Camera/CameraDemo.h"
#include "demos/tutorial/06_BasicLighting/BasicLighting.h"

LabLayer::LabLayer()
    : Layer("LabLayer")
{
}

LabLayer::~LabLayer() = default;

void LabLayer::OnAttach()
{
    Input::Initialize(Application::Get().GetWindow().GetNativeHandle());

    RegisterBuiltInDemos();

    if (!m_ActiveDemo)
    {
        // Default demo
        LOG_INFO_CAT(LogCategory::Demo, "Loading default demo: Clear Screen");
        auto defaultDemo = DemoRegistry::Create("01 - Clear Screen");
        RTRLAB_ASSERT_MSG(defaultDemo, "Failed to create default demo: 01 - Clear Screen");
        SetActiveDemo(std::move(defaultDemo), "01 - Clear Screen");
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
        auto demo = DemoRegistry::Create(name);
        if (!demo)
        {
            LOG_ERROR_CAT(LogCategory::Demo, "Failed to create demo: {}", name);
            return;
        }

        SetActiveDemo(std::move(demo), name);
        if (m_ActiveDemo)
            m_ActiveDemo->OnAttach();
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
    // --- Tutorial demos ---
    DemoRegistry::Register("01 - Clear Screen", []()
                           {
            const auto& window = Application::Get().GetWindow();
            return CreateScope<ClearScreen>(window.GetWidth(), window.GetHeight()); });

    DemoRegistry::Register("02 - Triangle", []()
                           {
            const auto& window = Application::Get().GetWindow();
            return CreateScope<Triangle>(window.GetWidth(), window.GetHeight()); });

    DemoRegistry::Register("03 - Textured Quad", []()
                           {
            const auto& window = Application::Get().GetWindow();
            return CreateScope<TexturedQuad>(window.GetWidth(), window.GetHeight()); });

    DemoRegistry::Register("04 - Transformed Cube", []()
                           {
            const auto& window = Application::Get().GetWindow();
            return CreateScope<TransformedCube>(window.GetWidth(), window.GetHeight()); });

    DemoRegistry::Register("05 - Camera", []()
                           {
            const auto& window = Application::Get().GetWindow();
            return CreateScope<CameraDemo>(window.GetWidth(), window.GetHeight()); });

    DemoRegistry::Register("06 - Basic Lighting", []()
                           {
            const auto& window = Application::Get().GetWindow();
            return CreateScope<BasicLighting>(window.GetWidth(), window.GetHeight()); });

    // --- Showcase demos ---
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
    if (!demo)
    {
        LOG_ERROR_CAT(LogCategory::Demo, "SetActiveDemo called with null demo: {}", name);
        return;
    }

    if (m_ActiveDemo)
        m_ActiveDemo->OnDetach();

    m_ActiveDemo = std::move(demo);
    m_ActiveDemoName = name;
    LOG_INFO_CAT(LogCategory::Demo, "Active demo: {}", m_ActiveDemoName);

    Application::Get().GetEventBus().Publish(
        DemoSwitchedEvent{m_SelectedDemoIndex, m_ActiveDemoName.c_str()});
}
