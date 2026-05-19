#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include "GUI/Backends/Vulkan/VulkanImGuiBridge.h"

#include "Core/App/Application.h"
#include "Core/Diagnostics/Assert/Assert.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Input/Input.h"

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

void ImGuiLayer::OnAttach()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
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
    GLFWwindow* window = Application::Get().GetWindow().GetNativeHandle();
    ImGui_ImplGlfw_InitForOther(window, true);
    VulkanImGuiBridge::Init(Application::Get().GetDevice(), Application::Get().GetSwapchain());
}

void ImGuiLayer::OnDetach()
{
    VulkanImGuiBridge::Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::Begin()
{
    VulkanImGuiBridge::NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Forward ImGui's capture state to the Input polling layer so that
    // game/demo code does not respond to keys/mouse meant for UI widgets.
    ImGuiIO& io = ImGui::GetIO();
    Input::SetKeyboardCaptured(io.WantCaptureKeyboard);
    Input::SetMouseCaptured(io.WantCaptureMouse);
}

void ImGuiLayer::End()
{
    ImGui::Render();

    auto& app = Application::Get();
    Texture* swapchainImage = app.GetCurrentSwapchainImage();
    CommandList* commandList = app.GetCurrentCommandList();
    RTRLAB_ASSERT_MSG(swapchainImage != nullptr, "ImGui rendering requires an acquired swapchain image.");
    RTRLAB_ASSERT_MSG(commandList != nullptr, "ImGui rendering requires an active command list.");

    app.GetResourceStateTracker().Transition(swapchainImage, TextureState::RenderTarget);
    app.GetResourceStateTracker().FlushBarriers(commandList);

    VulkanImGuiBridge::RenderDrawData(ImGui::GetDrawData(), commandList, app.GetCurrentSwapchainImageView());
}
