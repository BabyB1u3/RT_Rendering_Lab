#include "LayerStack.h"

#include <algorithm>

LayerStack::~LayerStack()
{
    for (auto &layer : m_Layers)
    {
        if (layer)
            layer->OnDetach();
    }
}

Layer *LayerStack::PushLayer(Scope<Layer> layer)
{
    Layer *raw = layer.get();
    m_Layers.emplace(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), std::move(layer));
    ++m_LayerInsertIndex;
    raw->OnAttach();
    return raw;
}

Layer *LayerStack::PushOverlay(Scope<Layer> overlay)
{
    Layer *raw = overlay.get();
    m_Layers.emplace_back(std::move(overlay));
    raw->OnAttach();
    return raw;
}

void LayerStack::PopLayer(Layer *layer)
{
    auto it = std::find_if(m_Layers.begin(), m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex),
                           [layer](const Scope<Layer> &ptr)
                           { return ptr.get() == layer; });

    if (it != m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex))
    {
        (*it)->OnDetach();
        m_Layers.erase(it);
        --m_LayerInsertIndex;
    }
}

void LayerStack::PopOverlay(Layer *overlay)
{
    auto it = std::find_if(m_Layers.begin() + static_cast<std::ptrdiff_t>(m_LayerInsertIndex), m_Layers.end(),
                           [overlay](const Scope<Layer> &ptr)
                           { return ptr.get() == overlay; });

    if (it != m_Layers.end())
    {
        (*it)->OnDetach();
        m_Layers.erase(it);
    }
}
