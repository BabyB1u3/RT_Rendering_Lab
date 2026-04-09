#pragma once

#include <memory>
#include <string>

#include "Core/App/Layer.h"

struct LayerLifecycleState
{
    int AttachCount = 0;
    int DetachCount = 0;
};

class LifecycleTrackingLayer : public Layer
{
public:
    LifecycleTrackingLayer(const std::string &name, std::shared_ptr<LayerLifecycleState> state)
        : Layer(name), m_State(std::move(state))
    {
    }

    void OnAttach() override
    {
        ++m_State->AttachCount;
    }

    void OnDetach() override
    {
        ++m_State->DetachCount;
    }

private:
    std::shared_ptr<LayerLifecycleState> m_State;
};
