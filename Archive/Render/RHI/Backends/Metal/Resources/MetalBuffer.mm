#include "Render/RHI/Backends/Metal/Resources/MetalBuffer.h"

#include "Core/Diagnostics/Assert/Assert.h"

MetalBuffer& GetMetalBuffer(Buffer* buffer)
{
    auto* metalBuffer = dynamic_cast<MetalBuffer*>(buffer);
    RTRLAB_ASSERT_MSG(metalBuffer != nullptr, "Buffer is not owned by the Metal backend.");
    return *metalBuffer;
}
