#include "core/app/Application.h"

#include <GLFW/glfw3.h>

#include "core/FileSystem.h"
#include "core/diagnostics/Assert.h"
#include "core/diagnostics/CrashHandler.h"
#include "core/diagnostics/FrameFormatter.h"
#include "core/diagnostics/LogCategories.h"
#include "core/diagnostics/LogMacros.h"
#include "core/diagnostics/Logger.h"
#include "core/event/Events.h"
#include "core/input/Input.h"
#include "core/Time.h"
#include "gui/ImGuiLayer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#ifdef GLAB_BACKEND_METAL
#include "graphics/backends/metal/MetalGraphicsDevice.h"
#else
#include "graphics/backends/opengl/GLGraphicsDevice.h"
#endif

Application *Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification &spec)
{
    FileSystem::Init();
    Diagnostics::Logger::Init(FileSystem::GetSavedPath("logs/RTRLab.log"));
    Diagnostics::CrashHandler::Init();
    LOG_INFO_CAT(LogCategory::FileSystem, "FileSystem initialized - root: {}", FileSystem::GetRootPath().string());
    LOG_INFO_CAT(LogCategory::FileSystem, "Saved directory: {}", FileSystem::GetSavedDir().string());
    LOG_INFO("Starting application: {}", spec.Name);

    RTRLAB_ASSERT_MSG(!s_Instance, "Application already exists.");

    WindowProps props;
    props.Title = spec.Name;
    props.Width = spec.Width;
    props.Height = spec.Height;
    props.VSync = spec.VSync;

    m_Window = CreateScope<Window>(props);
    m_Window->SetRefreshCallback([this]()
                                 {
        m_FrameRenderedThisTick = true;
        Time::Update(glfwGetTime());
        if (!m_Minimized)
            RenderFrame(); });

    // Subscribe to resize events BEFORE SetEventBus() installs the GLFW callbacks
    // that publish them, so no event is missed.
    m_ResizeConnection = m_EventBus.Subscribe<WindowResizeEvent>(
        [this](const WindowResizeEvent &e)
        { OnWindowResize(e.Width, e.Height); });
    m_Window->SetEventBus(&m_EventBus);

#ifdef GLAB_BACKEND_METAL
    SetDevice(CreateRef<MetalGraphicsDevice>(m_Window->GetNativeHandle()));
#else
    SetDevice(CreateRef<GLGraphicsDevice>());
#endif
    RenderCommand::Init();

    Input::Initialize(m_Window->GetNativeHandle());
    Time::Reset();

    s_Instance = this;

    auto imguiLayer = CreateScope<ImGuiLayer>();
    m_ImGuiLayer = imguiLayer.get();
    PushOverlay(std::move(imguiLayer));

    LOG_INFO("Application initialized");
}

Application::~Application()
{
    LOG_INFO("Application shutting down");
    Diagnostics::Logger::Shutdown();
    s_Instance = nullptr;
}

void Application::RenderFrame()
{
    Diagnostics::IncrementFrameNumber();

    // P1: Begin frame - Metal/Vulkan create command buffer here.
    RenderCommand::BeginFrame();

    // Phase 1: logic update (input, physics, animation, etc.)
    for (auto &layer : m_LayerStack)
        layer->OnUpdate(Time::GetDeltaTime());

    // Phase 2: GPU draw calls (scene rendering)
    for (auto &layer : m_LayerStack)
        layer->OnRender();

    // Phase 3: ImGui pass - Begin/End bracket all OnImGuiRender() calls
    // so that ImGui's NewFrame/Render are issued exactly once per frame.
    m_ImGuiLayer->Begin();
    for (auto &layer : m_LayerStack)
        layer->OnImGuiRender();
    m_ImGuiLayer->End();

    // P1: End frame - Metal/Vulkan commit command buffer and present here.
    RenderCommand::EndFrame();

    m_Window->SwapBuffers();
}

void Application::Run()
{
    while (m_Running && !m_Window->ShouldClose())
    {
        m_Window->PollEvents();
        Input::BeginFrame();

        const double currentTime = glfwGetTime();
        Time::Update(currentTime);

        // Skip all layer processing while minimized - no visible surface to render to,
        // and some drivers return a 0x0 framebuffer which would cause GL errors.
        // Also skip if the window refresh callback already rendered a frame this tick
        // (happens on macOS during live resize).
        if (!m_Minimized && !m_FrameRenderedThisTick)
            RenderFrame();

        m_FrameRenderedThisTick = false;
    }
}

void Application::Close()
{
    m_Running = false;
}

Layer *Application::PushLayer(Scope<Layer> layer)
{
    return m_LayerStack.PushLayer(std::move(layer));
}

Layer *Application::PushOverlay(Scope<Layer> overlay)
{
    return m_LayerStack.PushOverlay(std::move(overlay));
}

void Application::OnWindowResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_Minimized = true;
        return;
    }

    m_Minimized = false;
    RenderCommand::SetViewport(0, 0, width, height);
    GetDevice()->OnResize(width, height);

    for (auto &layer : m_LayerStack)
        layer->OnResize(width, height);
}
