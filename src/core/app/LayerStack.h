#pragma once

/// @file LayerStack.h
/// @brief Owns and orders all Layer instances for the application main loop.
///
/// The stack is split into two regions within a single vector:
///
///   [ Layer 0 | Layer 1 | ... | Overlay 0 | Overlay 1 | ... ]
///                              ^
///                        m_LayerInsertIndex
///
/// - Layers are inserted before the split point (front half).
/// - Overlays are appended after the split point (back half).
///
/// Iteration order (begin → end) guarantees that regular layers are updated
/// before overlays, so overlays (e.g., ImGuiLayer) always render on top.
///
/// The LayerStack owns all layers via Scope<Layer> and calls OnDetach()
/// on every layer when the stack is destroyed.

#include <vector>
#include <cstddef>

#include "core/app/Layer.h"
#include "core/Base.h"

class LayerStack
{
public:
    LayerStack() = default;
    ~LayerStack();

    /// Insert a regular layer before all overlays. Calls OnAttach() immediately.
    /// Returns a non-owning pointer for later reference.
    Layer *PushLayer(Scope<Layer> layer);

    /// Append an overlay after all regular layers. Calls OnAttach() immediately.
    /// Returns a non-owning pointer for later reference.
    Layer *PushOverlay(Scope<Layer> overlay);

    /// Remove a regular layer. Calls OnDetach() and releases ownership.
    void PopLayer(Layer *layer);
    /// Remove an overlay. Calls OnDetach() and releases ownership.
    void PopOverlay(Layer *overlay);

    auto begin() { return m_Layers.begin(); }
    auto end() { return m_Layers.end(); }

    auto begin() const { return m_Layers.begin(); }
    auto end() const { return m_Layers.end(); }

    auto rbegin() { return m_Layers.rbegin(); }
    auto rend() { return m_Layers.rend(); }

    auto rbegin() const { return m_Layers.rbegin(); }
    auto rend() const { return m_Layers.rend(); }

private:
    std::vector<Scope<Layer>> m_Layers;
    /// Index that separates regular layers (before) from overlays (after).
    std::size_t m_LayerInsertIndex = 0;
};
