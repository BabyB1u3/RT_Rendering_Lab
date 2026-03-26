#pragma once

/// @file Application.h
/// @brief Top-level application class — owns the window, layer stack, and main loop.
///
/// Construction order:
///   Logger::Init() → FileSystem::Init() → Window → Input → Time → ImGuiLayer (overlay)
///
/// Main loop (Application::Run):
///   1. Poll OS events
///   2. Update frame time
///   3. For each layer: OnUpdate(dt) → OnRender()
///   4. ImGuiLayer::Begin() → For each layer: OnImGuiRender() → ImGuiLayer::End()
///   5. Swap buffers
///
/// Only one Application instance may exist at a time (enforced by s_Instance).

#include <string>
#include <cstdint>

#include "core/Base.h"
#include "core/event/EventBus.h"
#include "core/app/Window.h"
#include "core/app/LayerStack.h"

class ImGuiLayer;

struct ApplicationSpecification
{
    std::string Name = "RTRLab";
    uint32_t Width = 1600;
    uint32_t Height = 900;
    bool VSync = true;
};

class Application
{
public:
    explicit Application(const ApplicationSpecification &spec = {});
    virtual ~Application();

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    /// Enter the main loop. Blocks until the window is closed or Close() is called.
    void Run();
    /// Signal the main loop to exit after the current frame.
    void Close();

    /// Push a regular layer (updated before overlays). Transfers ownership.
    Layer *PushLayer(Scope<Layer> layer);
    /// Push an overlay layer (updated after regular layers, renders on top). Transfers ownership.
    Layer *PushOverlay(Scope<Layer> overlay);

    Window &GetWindow() { return *m_Window; }
    const Window &GetWindow() const { return *m_Window; }

    EventBus &GetEventBus() { return m_EventBus; }

    static Application &Get() { return *s_Instance; }

private:
    /// Handles window resize: updates viewport and notifies all layers.
    void OnWindowResize(uint32_t width, uint32_t height);
    /// Render one full frame (update → render → ImGui). Called from Run() and
    /// the window refresh callback (macOS live resize).
    void RenderFrame();

private:
    // Non-owning singleton-style pointer. Lifetime is managed externally by the actual Application object.
    static Application *s_Instance;

    Scope<Window> m_Window;
    EventBus m_EventBus;
    LayerStack m_LayerStack;
    // Non-owning pointer. Lifetime is managed by the LayerStack (pushed as overlay).
    ImGuiLayer *m_ImGuiLayer = nullptr;

    bool m_Running = true;
    bool m_Minimized = false;
    // Set to true when the window refresh callback already rendered a frame
    // so that the main loop can skip the redundant render for that tick.
    bool m_FrameRenderedThisTick = false;
};
