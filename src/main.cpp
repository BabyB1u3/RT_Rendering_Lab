#include "core/Application.h"
#include "core/Base.h"
#include "demos/LabLayer.h"

int main()
{
    ApplicationSpecification spec;
    spec.Name = "RTRLab";
    spec.Width = 1600;
    spec.Height = 900;
    spec.VSync = true;

    Application app(spec);
    app.PushLayer(CreateScope<LabLayer>());
    app.Run();

    return 0;
}