#include "Render/RHI/Backends/Metal/Pipeline/MetalGraphicsPipeline.h"

#include "Core/Diagnostics/Assert/Assert.h"

const MetalGraphicsPipeline& GetMetalGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
{
    auto* metalGraphicsPipeline = dynamic_cast<MetalGraphicsPipeline*>(graphicsPipeline);
    RTRLAB_ASSERT_MSG(metalGraphicsPipeline != nullptr, "Graphics pipeline is not owned by the Metal backend.");
    return *metalGraphicsPipeline;
}
