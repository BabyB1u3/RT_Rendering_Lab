#pragma once

#include <string>
#include <cstdint>

#include "core/Base.h"
#include "core/Window.h"
#include "core/LayerStack.h"

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

    void Run();
    void Close();

    Layer *PushLayer(Scope<Layer> layer);
    Layer *PushOverlay(Scope<Layer> overlay);

    Window &GetWindow() { return *m_Window; }
    const Window &GetWindow() const { return *m_Window; }

    static Application &Get() { return *s_Instance; }

private:
    void OnWindowResize(uint32_t width, uint32_t height);

private:
    static Application *s_Instance;

    Scope<Window> m_Window;
    LayerStack m_LayerStack;

    bool m_Running = true;
    bool m_Minimized = false;
};
