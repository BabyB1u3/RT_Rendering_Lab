#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#ifndef GLAB_BACKEND_METAL
#include <imgui_impl_opengl3.h>
#endif

#include "core/app/Application.h"
#include "core/FileSystem.h"
#include "core/input/Input.h"

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
    static std::string iniPath = FileSystem::GetSavedConfigPath("imgui.ini").string();
    io.IniFilename = iniPath.c_str();

    ImGui::StyleColorsDark();

    // install_callbacks = true lets ImGui intercept GLFW input events.
    GLFWwindow *window = Application::Get().GetWindow().GetNativeHandle();
#ifdef GLAB_BACKEND_METAL
    ImGui_ImplGlfw_InitForOther(window, true);
#else
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    // GLSL 460 matches the OpenGL 4.6 core context created in Window.
    ImGui_ImplOpenGL3_Init("#version 460");
#endif
}

void ImGuiLayer::OnDetach()
{
#ifndef GLAB_BACKEND_METAL
    ImGui_ImplOpenGL3_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::Begin()
{
#ifndef GLAB_BACKEND_METAL
    ImGui_ImplOpenGL3_NewFrame();
#endif
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
#ifndef GLAB_BACKEND_METAL
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}
