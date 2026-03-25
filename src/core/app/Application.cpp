#include "core/app/Application.h"

#include <stdexcept>

#include <GLFW/glfw3.h>

#include "core/FileSystem.h"
#include "core/input/Input.h"
#include "core/Logger.h"
#include "core/Time.h"
#include "gui/ImGuiLayer.h"
#include "graphics/GraphicsDevice.h"
#include "graphics/RenderCommand.h"
#include "graphics/opengl/GLGraphicsDevice.h"

Application *Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification &spec)
{
    Logger::Init();
    FileSystem::Init();
    LOG_INFO("Starting application: {}", spec.Name);

    if (s_Instance)
        throw std::runtime_error("Application already exists.");

    WindowProps props;
    props.Title = spec.Name;
    props.Width = spec.Width;
    props.Height = spec.Height;
    props.VSync = spec.VSync;

    m_Window = CreateScope<Window>(props);
    m_Window->SetResizeCallback([this](uint32_t width, uint32_t height)
                                { OnWindowResize(width, height); });
    m_Window->SetEventBus(&m_EventBus);

    SetDevice(CreateRef<GLGraphicsDevice>());
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
    s_Instance = nullptr;
}

void Application::Run()
{
    while (m_Running && !m_Window->ShouldClose())
    {
        m_Window->PollEvents();
        Input::BeginFrame();

        const double currentTime = glfwGetTime();
        Time::Update(currentTime);

        // Skip all layer processing while minimized — no visible surface to render to,
        // and some drivers return a 0×0 framebuffer which would cause GL errors.
        if (!m_Minimized)
        {
            // P1: Begin frame — Metal/Vulkan create command buffer here.
            RenderCommand::BeginFrame();

            // Phase 1: logic update (input, physics, animation, etc.)
            for (auto &layer : m_LayerStack)
                layer->OnUpdate(Time::GetDeltaTime());

            // Phase 2: GPU draw calls (scene rendering)
            for (auto &layer : m_LayerStack)
                layer->OnRender();

            // Phase 3: ImGui pass — Begin/End bracket all OnImGuiRender() calls
            // so that ImGui's NewFrame/Render are issued exactly once per frame.
            m_ImGuiLayer->Begin();
            for (auto &layer : m_LayerStack)
                layer->OnImGuiRender();
            m_ImGuiLayer->End();

            // P1: End frame — Metal/Vulkan commit command buffer and present here.
            RenderCommand::EndFrame();
        }

        m_Window->SwapBuffers();
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

    for (auto &layer : m_LayerStack)
        layer->OnResize(width, height);
}
