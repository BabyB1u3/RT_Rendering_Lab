#pragma once

/// @file DemoRenderUtils.h
/// @brief Small shared helpers for the tutorial demo ladder.

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "Core/Util/Base.h"
#include "Core/Util/Math.h"
#include "Render/RHI/RHI.h"
#include "Render/Shader/ShaderCompiler.h"

namespace DemoRenderUtils
{
struct DemoViewport
{
    float m_X = 0.0f;
    float m_Y = 0.0f;
    float m_Width = 0.0f;
    float m_Height = 0.0f;
};

struct LoadedImage
{
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
    std::vector<uint8_t> m_Pixels;
};

struct ColoredVertex
{
    Math::Vec4 m_Position;
    Math::Vec4 m_Color;
};

struct TexturedVertex
{
    Math::Vec4 m_Position;
    Math::Vec2 m_UV;
};

Math::Mat4 BuildOrbitViewProjection(uint32_t framebufferWidth, uint32_t framebufferHeight);

DemoViewport ComputeCenteredSquareViewport(uint32_t framebufferWidth, uint32_t framebufferHeight);
uint32_t
FindRequiredSetIndex(const PipelineLayoutDesc& layoutDesc, std::string_view bindingName, std::string_view debugOwner);

ShaderCompileRequest BuildGraphicsShaderCompileRequest(const std::filesystem::path& shaderPath);
CompiledShaderProgramDesc CompileShaderProgramDesc(const ShaderCompileRequest& request, std::string_view debugOwner);

void CreateStaticBufferPair(Device& device,
                            const void* data,
                            uint64_t size,
                            BufferUsage targetUsage,
                            const char* targetDebugName,
                            const char* uploadDebugName,
                            Scope<Buffer>& targetBuffer,
                            Scope<Buffer>& uploadBuffer);

void UploadStaticBufferPair(CommandList& commandList,
                            ResourceStateTracker& resourceStateTracker,
                            Buffer* uploadBuffer,
                            Buffer* targetBuffer,
                            uint64_t size,
                            BufferState finalState);

LoadedImage LoadTextureFileRGBA8(const std::filesystem::path& texturePath, std::string_view debugOwner);
} // namespace DemoRenderUtils
