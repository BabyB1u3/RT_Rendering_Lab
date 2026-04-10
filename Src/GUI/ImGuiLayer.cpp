#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#if defined(GLAB_BACKEND_OPENGL)
#include <imgui_impl_opengl3.h>
#elif defined(GLAB_BACKEND_METAL)
// #include "GUI/Backends/Metal/MetalImGuiBridge.h"
#elif defined(GLAB_BACKEND_VULKAN)
// Vulkan renderer backend integration will be wired in once the RHI path lands.
#else
#error Unsupported graphics backend
#endif

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Input/Input.h"

ImGuiLayer::ImGuiLayer()
    : Layer("ImGuiLayer")
{
}

void ImGuiLayer::OnAttach()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Store imgui.ini in the user config directory (persistent static string
    // because ImGui holds a raw pointer to IniFilename).
    const auto iniPath = FileSystem::ResolveWritePath("/Saved/Config/imgui.ini");
    RTRLAB_ASSERT_MSG(iniPath.has_value(), "Failed to resolve /Saved/Config/imgui.ini");
    static std::string s_IniPath;
    s_IniPath = iniPath ? iniPath->string() : std::string{};
    io.IniFilename = s_IniPath.empty() ? nullptr : s_IniPath.c_str();

    ImGui::StyleColorsDark();

    // install_callbacks = true lets ImGui intercept GLFW input events.
    GLFWwindow *window = Application::Get().GetWindow().GetNativeHandle();
#if defined(GLAB_BACKEND_OPENGL)
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    // GLSL 460 matches the OpenGL 4.6 core context created in Window.
    ImGui_ImplOpenGL3_Init("#version 460");
#elif defined(GLAB_BACKEND_METAL)
    ImGui_ImplGlfw_InitForOther(window, true);
    // auto *metalDevice = static_cast<MetalGraphicsDevice *>(GetDevice().get());
    // MetalImGuiBridge::Init(metalDevice->GetMTLDevice());
#elif defined(GLAB_BACKEND_VULKAN)
    ImGui_ImplGlfw_InitForOther(window, true);
#endif
}

void ImGuiLayer::OnDetach()
{
#if defined(GLAB_BACKEND_OPENGL)
    ImGui_ImplOpenGL3_Shutdown();
#elif defined(GLAB_BACKEND_METAL)
    // MetalImGuiBridge::Shutdown();
#elif defined(GLAB_BACKEND_VULKAN)
    // Vulkan renderer backend shutdown will be added with the Vulkan renderer path.
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::Begin()
{
#if defined(GLAB_BACKEND_OPENGL)
    ImGui_ImplOpenGL3_NewFrame();
#elif defined(GLAB_BACKEND_METAL)
    // auto *metalDevice = static_cast<MetalGraphicsDevice *>(GetDevice().get());
    // metalDevice->GetMetalRenderCommand()->BeginImGuiFrame();
#elif defined(GLAB_BACKEND_VULKAN)
    // Vulkan renderer backend frame setup will be added with the Vulkan renderer path.
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Forward ImGui's capture state to the Input polling layer so that
    // game/demo code does not respond to keys/mouse meant for UI widgets.
    ImGuiIO &io = ImGui::GetIO();
    Input::SetKeyboardCaptured(io.WantCaptureKeyboard);
    Input::SetMouseCaptured(io.WantCaptureMouse);
}

void ImGuiLayer::End()
{
    ImGui::Render();
#if defined(GLAB_BACKEND_OPENGL)
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#elif defined(GLAB_BACKEND_METAL)
    // auto *metalDevice = static_cast<MetalGraphicsDevice *>(GetDevice().get());
    // metalDevice->GetMetalRenderCommand()->RenderImGui(ImGui::GetDrawData());
#elif defined(GLAB_BACKEND_VULKAN)
    // Vulkan renderer backend draw submission will be added with the Vulkan renderer path.
#endif
}
