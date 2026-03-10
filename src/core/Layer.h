#pragma once

#include <string>
#include <cstdint>

class Layer
{
public:
    explicit Layer(std::string name = "Layer");
    virtual ~Layer() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}

    virtual void OnUpdate(float dt) {}
    virtual void OnRender() {}
    virtual void OnImGuiRender() {}
    virtual void OnResize(uint32_t width, uint32_t height) {}

    const std::string &GetName() const { return m_Name; }

protected:
    std::string m_Name;
};
