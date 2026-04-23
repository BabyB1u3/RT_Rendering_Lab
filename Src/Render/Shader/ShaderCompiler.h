#pragma once

/// @file ShaderCompiler.h
/// @brief ShaderSystem compilation boundary types and compiler interface.

#include <string>
#include <string_view>
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

struct BuildShaderReflectionResult
{
    bool m_Succeeded = false;
    ShaderReflectionData m_Reflection;
    std::string m_ErrorMessage;
};

bool ValidateShaderCompileRequest(const ShaderCompileRequest& request, std::string* errorMessage = nullptr);
bool ValidateCompiledShaderProgramDesc(const CompiledShaderProgramDesc& program, std::string* errorMessage = nullptr);
BuildShaderReflectionResult BuildShaderReflectionFromSlangJson(std::string_view jsonText);
bool PopulateProgramReflectionFromSlangJson(CompiledShaderProgramDesc& program,
                                            std::string_view jsonText,
                                            std::string* errorMessage = nullptr);

class ShaderCompiler
{
public:
    virtual ~ShaderCompiler() = default;

    ShaderCompileResult CompileProgram(const ShaderCompileRequest& request) const;

protected:
    virtual ShaderCompileResult CompileProgramImpl(const ShaderCompileRequest& request) const = 0;
};

Scope<ShaderCompiler> CreateShaderCompiler();
