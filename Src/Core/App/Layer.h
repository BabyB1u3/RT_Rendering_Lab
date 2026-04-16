#pragma once

/// @file Layer.h
/// @brief Abstract base class for application layers.
///
/// A Layer represents a distinct stage of per-frame processing (e.g., a demo,
/// an overlay, or a debug tool). Layers are managed by LayerStack and driven
/// by the Application main loop in a fixed order:
///   OnUpdate(dt) → OnRender() → OnImGuiRender()
///
/// Lifecycle:
///   OnAttach()  - called once when the layer is pushed onto the stack.
///   OnDetach()  - called once when the layer is removed or the stack is destroyed.

#include <string>
#include <cstdint>

class Layer
{
public:
    explicit Layer(std::string name = "Layer");
    virtual ~Layer() = default;

    /// Called once when the layer is added to the LayerStack.
    virtual void OnAttach() {}
    /// Called once when the layer is removed from the LayerStack.
    virtual void OnDetach() {}

    /// Called every frame with the time elapsed since the last frame (in seconds).
    virtual void OnUpdate(double) {}
    /// Called every frame after OnUpdate - issue draw calls here.
    virtual void OnRender() {}
    /// Called every frame between ImGuiLayer::Begin() and End() - draw ImGui widgets here.
    virtual void OnImGuiRender() {}
    /// Called when the window is resized (width and height in pixels).
    virtual void OnResize(uint32_t, uint32_t) {}

    const std::string& GetName() const { return m_Name; }

protected:
    std::string m_Name;
};
