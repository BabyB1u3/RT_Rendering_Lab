#pragma once

/// @file ImGuiLayer.h
/// @brief Layer subclass that manages the Dear ImGui lifecycle.
///
/// Pushed as an *overlay* on the LayerStack so it renders on top of all normal
/// layers. OnAttach() initializes the ImGui context and GLFW/OpenGL3 backends.
/// OnDetach() tears them down.
///
/// Frame bracketing:
///   Application::Run() calls Begin() before any layer's OnImGuiRender(),
///   then End() after all layers have submitted their ImGui draw commands.
///   Begin() starts a new ImGui frame; End() calls ImGui::Render() and
///   dispatches the draw data to the OpenGL3 backend.

#include "core/app/Layer.h"

class ImGuiLayer : public Layer
{
public:
    ImGuiLayer();
    ~ImGuiLayer() override = default;

    void OnAttach() override;
    void OnDetach() override;

    /// Start a new ImGui frame. Must be called once before any OnImGuiRender().
    void Begin();
    /// Finalize and render all ImGui draw data. Called once after all OnImGuiRender().
    void End();
};
