#include "Core/App/Application.h"

#include <GLFW/glfw3.h>

#include "Core/Resource/FileSystem.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Diagnostics/Crash/CrashHandler.h"
#include "Core/Diagnostics/Logging/FrameFormatter.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Diagnostics/Logging/Logger.h"
#include "Core/Event/Events.h"
#include "Core/Input/Input.h"
#include "Core/Util/Time.h"
#include "GUI/ImGuiLayer.h"

Application *Application::s_Instance = nullptr;

Application::Application(const ApplicationSpecification &spec)
{
    FileSystem::Init();
    Diagnostics::Logger::Init();
    Diagnostics::CrashHandler::Init();
    LOG_INFO_CAT(LogCategory::FileSystem, "FileSystem initialized - root: {}", FileSystem::GetRootPath().string());
    LOG_INFO_CAT(LogCategory::FileSystem, "Saved directory: {}", FileSystem::GetSavedDir().string());
    LOG_INFO_CAT(LogCategory::Core, "Starting application: {}", spec.Name);

    RTRLAB_ASSERT_MSG(!s_Instance, "Application already exists.");

    WindowProps props;
    props.Title = spec.Name;
    props.Width = spec.Width;
    props.Height = spec.Height;

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

    Input::Initialize(m_Window->GetNativeHandle());
    Input::SetEventBus(&m_EventBus);
    Time::Reset();
    InitializeRHI();

    s_Instance = this;

    // auto imguiLayer = CreateScope<ImGuiLayer>();
    // m_ImGuiLayer = imguiLayer.get();
    // PushOverlay(std::move(imguiLayer));

    LOG_INFO_CAT(LogCategory::Core, "Application initialized");
}

Application::~Application()
{
    LOG_INFO_CAT(LogCategory::Core, "Application shutting down");

    m_LayerStack.Clear();
    // m_ImGuiLayer = nullptr;
    m_Swapchain.reset();
    m_Device.reset();
    m_Window.reset();

    Diagnostics::Logger::Shutdown();
    s_Instance = nullptr;
}

void Application::RenderFrame()
{
    Diagnostics::IncrementFrameNumber();

    BeginFrame();

    // Phase 1: logic update (input, physics, animation, etc.)
    for (auto &layer : m_LayerStack)
        layer->OnUpdate(Time::GetDeltaTime());

    // Phase 2: GPU draw calls (scene rendering)
    for (auto &layer : m_LayerStack)
        layer->OnRender();

    // Phase 3: ImGui pass - Begin/End bracket all OnImGuiRender() calls
    // so that ImGui's NewFrame/Render are issued exactly once per frame.
    // m_ImGuiLayer->Begin();
    // for (auto &layer : m_LayerStack)
    //     layer->OnImGuiRender();
    // m_ImGuiLayer->End();

    EndFrame();
    PresentFrame();
}

void Application::Run()
{
    while (m_Running && !m_Window->ShouldClose())
    {
        m_Window->PollEvents();

        const double currentTime = glfwGetTime();
        Time::Update(currentTime);
        Input::BeginFrame(Time::GetDeltaTime());

        // Skip all layer processing while minimized - no visible surface to render to,
        // and some drivers return a 0x0 framebuffer which would cause GL errors.
        // Also skip if the window refresh callback already rendered a frame this tick
        // (happens on macOS during live resize).
        if (ShouldRenderFrame())
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

    if (m_Swapchain)
        m_Swapchain->resize(width, height);

    for (auto &layer : m_LayerStack)
        layer->OnResize(width, height);
}

bool Application::ShouldRenderFrame() const
{
    return !m_Minimized && !m_FrameRenderedThisTick;
}

void Application::BeginFrame()
{
    RTRLAB_ASSERT_MSG(m_Device && m_Swapchain, "Application RHI must be initialized before beginning a frame.");

    m_FrameContext = m_Device->beginFrame();
    m_SwapchainImageIndex = m_Swapchain->acquireNextImage();
    m_SwapchainImage = m_Swapchain->getImage(m_SwapchainImageIndex);
    m_SwapchainImageView = m_Swapchain->getImageView(m_SwapchainImageIndex);
    m_CommandList = m_Device->beginCommandList();

    m_ResourceStateTracker.transition(m_SwapchainImage, TextureState::RenderTarget);
    m_ResourceStateTracker.flushBarriers(m_CommandList);

    ColorAttachmentInfo colorAttachment;
    colorAttachment.view = m_SwapchainImageView;
    colorAttachment.loadOp = LoadOp::Clear;
    colorAttachment.storeOp = StoreOp::Store;
    colorAttachment.clearValue = {0.08f, 0.10f, 0.12f, 1.0f};

    RenderingInfo renderingInfo;
    renderingInfo.colorAttachments = {colorAttachment};
    renderingInfo.renderArea = {0, 0, m_Swapchain->width(), m_Swapchain->height()};

    m_CommandList->beginRendering(renderingInfo);
}

void Application::EndFrame()
{
    RTRLAB_ASSERT_MSG(m_Device && m_Swapchain, "Application RHI must remain valid until the frame ends.");
    RTRLAB_ASSERT_MSG(m_CommandList && m_FrameContext, "EndFrame requires an active frame and command list.");

    m_CommandList->endRendering();

    m_ResourceStateTracker.transition(m_SwapchainImage, TextureState::Present);
    m_ResourceStateTracker.flushBarriers(m_CommandList);

    m_Device->submit(m_CommandList);
    m_Device->endFrame(m_FrameContext);

    m_FrameContext = nullptr;
    m_CommandList = nullptr;
}

void Application::PresentFrame()
{
    RTRLAB_ASSERT_MSG(m_Swapchain, "Application swapchain must remain valid until presentation.");
    RTRLAB_ASSERT_MSG(m_SwapchainImageView, "PresentFrame requires a valid swapchain image view from BeginFrame.");

    m_Swapchain->present(m_SwapchainImageIndex);
    m_ResourceStateTracker.reset();
    m_SwapchainImageIndex = std::numeric_limits<uint32_t>::max();
    m_SwapchainImage = nullptr;
    m_SwapchainImageView = nullptr;
}

void Application::InitializeRHI()
{
    m_Device = createDefaultDevice();

    SwapchainDesc swapchainDesc;
    swapchainDesc.width = m_Window->GetWidth();
    swapchainDesc.height = m_Window->GetHeight();
    swapchainDesc.format = Format::BGRA8_UNORM;
    swapchainDesc.imageCount = 2;
    swapchainDesc.vsync = true;

    m_Swapchain = m_Device->createSwapchain(swapchainDesc, m_Window->GetNativeWindowHandle());
}
