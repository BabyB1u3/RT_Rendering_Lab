#pragma once

#include <memory>
#include <string>

#include "core/Layer.h"
#include "core/Base.h"

class DemoBase;

class LabLayer : public Layer
{
public:
    LabLayer();

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

    std::string m_ActiveDemoName;
    Scope<DemoBase> m_ActiveDemo;
};