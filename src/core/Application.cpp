#include "Application.h"

#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Input.h"
#include "Logger.h"
#include "Time.h"

Application *Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification &spec)
{
    Logger::Init();

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

    Input::Initialize(m_Window->GetNativeHandle());
    Time::Reset();

    s_Instance = this;
}

Application::~Application()
{
    s_Instance = nullptr;
}

void Application::Run()
{
    while (m_Running && !m_Window->ShouldClose())
    {
        m_Window->PollEvents();

        const double currentTime = glfwGetTime();
        Time::Update(currentTime);

        if (!m_Minimized)
        {
            for (auto &layer : m_LayerStack)
                layer->OnUpdate(Time::GetDeltaTime());

            for (auto &layer : m_LayerStack)
                layer->OnRender();

            for (auto &layer : m_LayerStack)
                layer->OnImGuiRender();
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
    glViewport(0, 0, static_cast<int>(width), static_cast<int>(height));

    for (auto &layer : m_LayerStack)
        layer->OnResize(width, height);
}
