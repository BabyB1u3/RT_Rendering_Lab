#include "GraphicsDevice.h"

#include "core/diagnostics/Assert.h"

static Ref<IGraphicsDevice> s_Device = nullptr;

void SetDevice(Ref<IGraphicsDevice> device)
{
	s_Device = std::move(device);
}

Ref<IGraphicsDevice> GetDevice()
{
	RTRLAB_ASSERT_MSG(s_Device, "GetDevice(): no graphics device set. Call SetDevice() first.");
	return s_Device;
}
