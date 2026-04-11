/// @file main.cpp
/// @brief Application entry point.
///
/// Creates the Application with a window spec, pushes LabLayer as the sole
/// Layer, and enters the main loop. LabLayer handles demo registration,
/// switching, and all per-frame callbacks.

#include "Core/App/Application.h"
#include "Core/Util/Base.h"
#include "Demos/LabLayer.h"

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
