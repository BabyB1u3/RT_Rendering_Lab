#include "Render/RHI/Backends/Metal/MetalDevice.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <string>

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "Core/Diagnostics/Assert/Assert.h"

#include "Render/RHI/Backends/Metal/MetalInternals.inl"
#include "Render/RHI/Backends/Metal/MetalCommandList.inl"
#include "Render/RHI/Backends/Metal/MetalSwapchain.inl"
#include "Render/RHI/Backends/Metal/MetalDeviceImpl.inl"
