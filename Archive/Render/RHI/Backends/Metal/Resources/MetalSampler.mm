#include "Render/RHI/Backends/Metal/Resources/MetalSampler.h"

#include "Core/Diagnostics/Assert/Assert.h"

const MetalSampler& GetMetalSampler(Sampler* sampler)
{
    auto* metalSampler = dynamic_cast<MetalSampler*>(sampler);
    RTRLAB_ASSERT_MSG(metalSampler != nullptr, "Sampler is not owned by the Metal backend.");
    return *metalSampler;
}
