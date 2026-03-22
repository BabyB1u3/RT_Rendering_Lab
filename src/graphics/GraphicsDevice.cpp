#include "GraphicsDevice.h"

#include <cassert>

static Ref<IGraphicsDevice> s_Device = nullptr;

void SetDevice(Ref<IGraphicsDevice> device)
{
	s_Device = std::move(device);
}

Ref<IGraphicsDevice> GetDevice()
{
	assert(s_Device && "GetDevice(): no graphics device set. Call SetDevice() first.");
	return s_Device;
}
