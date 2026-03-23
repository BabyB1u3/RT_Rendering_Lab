/// @file main.cpp
/// @brief Application entry point.
///
/// Creates the Application with a window spec, pushes LabLayer as the sole
/// Layer, and enters the main loop. LabLayer handles demo registration,
/// switching, and all per-frame callbacks.

#include "core/app/Application.h"
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
