#include <glad/glad.h>

#include "core/Application.h"
#include "core/Layer.h"

class ClearColorLayer : public Layer
{
public:
    ClearColorLayer()
        : Layer("ClearColorLayer")
    {
    }

    void OnRender() override
    {
        glClearColor(0.1f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
};

int main()
{
    ApplicationSpecification spec;
    spec.Name = "RTRLab";
    spec.Width = 1600;
    spec.Height = 900;
    spec.VSync = true;

    Application app(spec);
    app.PushLayer(CreateScope<ClearColorLayer>());
    app.Run();

    return 0;
}