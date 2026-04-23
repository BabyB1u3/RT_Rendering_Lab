#pragma once

/// @file Application.h
/// @brief Top-level application class - owns the window, layer stack, and main loop.
///
/// Construction order:
///   FileSystem::Init() - Diagnostics::Logger::Init() via /Saved/logs - Window - Input - Time - ImGuiLayer (overlay)
///
/// Main loop (Application::Run):
///   1. Poll OS events
///   2. Update frame time
///   3. Begin frame / acquire swapchain image / open command list
///   4. For each layer: OnUpdate(dt) -> OnRender()
///   5. ImGuiLayer::Begin() -> For each layer: OnImGuiRender() -> ImGuiLayer::End()
///   6. End frame / submit / present
///
/// Only one Application instance may exist at a time (enforced by s_Instance).

#include <string>
#include <cstdint>
#include <limits>
#include <vector>

#include "Core/Util/Base.h"
#include "Core/Event/EventBus.h"
#include "Core/Event/ScopedConnection.h"
#include "Core/App/Window/Window.h"
#include "Core/App/Layer/LayerStack.h"
#include "Render/RHI/RHI.h"

class ImGuiLayer;

struct ApplicationSpecification
{
    std::string m_Name = "RTRLab";
    uint32_t m_Width = 1600;
    uint32_t m_Height = 900;
};

class Application
{
public:
    explicit Application(const ApplicationSpecification& spec = {});
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Enter the main loop. Blocks until the window is closed or Close() is called.
    void Run();
    /// Signal the main loop to exit after the current frame.
    void Close();

    /// Push a regular layer (updated before overlays). Transfers ownership.
    Layer* PushLayer(Scope<Layer> layer);
    /// Push an overlay layer (updated after regular layers, renders on top). Transfers ownership.
    Layer* PushOverlay(Scope<Layer> overlay);

    Window& GetWindow() { return *m_Window; }
    const Window& GetWindow() const { return *m_Window; }
    Device& GetDevice() { return *m_Device; }
    const Device& GetDevice() const { return *m_Device; }
    Swapchain& GetSwapchain() { return *m_Swapchain; }
    const Swapchain& GetSwapchain() const { return *m_Swapchain; }
    ResourceStateTracker& GetResourceStateTracker() { return m_ResourceStateTracker; }
    const ResourceStateTracker& GetResourceStateTracker() const { return m_ResourceStateTracker; }

    EventBus& GetEventBus() { return m_EventBus; }
    /// Transitional per-frame recording accessors. These expose the active
    /// command list and swapchain backbuffer while the application still
    /// drives the frame pump, but pass ownership lives with the caller.
    CommandList* GetCurrentCommandList() const { return m_CommandList; }
    Texture* GetCurrentSwapchainImage() const { return m_SwapchainImage; }
    TextureView* GetCurrentSwapchainImageView() const { return m_SwapchainImageView; }

    static Application& Get() { return *s_Instance; }

private:
    /// Handles window resize: updates viewport and notifies all layers.
    void OnWindowResize(uint32_t width, uint32_t height);
    /// Render one full frame (begin frame -> update -> render -> ImGui -> end/present). Called from Run() and
    /// the window refresh callback (macOS live resize).
    void RenderFrame();
    /// Return whether the current loop tick should render a frame.
    bool ShouldRenderFrame() const;
    /// Begin the graphics frame by acquiring the swapchain image and opening a command list.
    void BeginFrame();
    /// End the graphics frame by finalizing swapchain state, submitting, and ending the backend frame.
    void EndFrame();
    /// Present the completed frame. Future explicit-RHI swapchain present lands here.
    void PresentFrame();
    /// Drop tracker state for the swapchain images currently known to the application.
    void ForgetTrackedSwapchainImages();
    /// Refresh the application's cached swapchain-image list after resize or backend recreation.
    void RefreshTrackedSwapchainImages();
    /// Create the backend-selected device and swapchain used by the application frame loop.
    void InitializeRHI();

private:
    // Non-owning singleton-style pointer. Lifetime is managed externally by the actual Application object.
    static Application* s_Instance;

    Scope<Window> m_Window;
    EventBus m_EventBus;
    LayerStack m_LayerStack;
    // Non-owning pointer. Lifetime is managed by the LayerStack (pushed as overlay).
    ImGuiLayer* m_ImGuiLayer = nullptr;

    bool m_Running = true;
    bool m_Minimized = false;
    // Set to true when the window refresh callback already rendered a frame
    // so that the main loop can skip the redundant render for that tick.
    bool m_FrameRenderedThisTick = false;

    Scope<Device> m_Device;
    Scope<Swapchain> m_Swapchain;
    ResourceStateTracker m_ResourceStateTracker;
    FrameContext* m_FrameContext = nullptr;
    CommandList* m_CommandList = nullptr;
    uint32_t m_SwapchainImageIndex = std::numeric_limits<uint32_t>::max();
    Texture* m_SwapchainImage = nullptr;
    TextureView* m_SwapchainImageView = nullptr;
    std::vector<Texture*> m_TrackedSwapchainImages;

    ScopedConnection m_ResizeConnection;
};
