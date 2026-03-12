#pragma once

#include <cstdint>

class DemoBase
{
public:
    virtual ~DemoBase() = default;

    virtual void OnAttach() {}
    virtual void OnDetach() {}

    virtual void OnUpdate(double) {}
    virtual void OnRender() {}
    virtual void OnImGuiRender() {}
    virtual void OnResize(uint32_t, uint32_t) {}
};