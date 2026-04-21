#pragma once

/// @file ShaderCompiler.h
/// @brief ShaderSystem compilation boundary types and compiler interface.

#include <string>
#include <vector>

#include "Core/Util/Base.h"
#include "Render/Shader/ShaderTypes.h"

struct ShaderCompileTargetDesc
{
    BackendType m_Backend = BackendType::Vulkan;
    MetalCodeFormat m_MetalCodeFormat = MetalCodeFormat::MslSource;
};

struct ShaderCompileRequest
{
    ShaderSourceDesc m_Source;
    std::vector<ShaderCompileTargetDesc> m_Targets;
};

struct ShaderCompileResult
{
    bool m_Succeeded = false;
    CompiledShaderProgramDesc m_Program;
    std::string m_ErrorMessage;
};

bool ValidateShaderCompileRequest(const ShaderCompileRequest& request, std::string* errorMessage = nullptr);
bool ValidateCompiledShaderProgramDesc(const CompiledShaderProgramDesc& program, std::string* errorMessage = nullptr);

class ShaderCompiler
{
public:
    virtual ~ShaderCompiler() = default;

    ShaderCompileResult CompileProgram(const ShaderCompileRequest& request) const;

protected:
    virtual ShaderCompileResult CompileProgramImpl(const ShaderCompileRequest& request) const = 0;
};

Scope<ShaderCompiler> CreateShaderCompiler();
