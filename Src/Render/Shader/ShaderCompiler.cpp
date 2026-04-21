#include "Render/Shader/ShaderCompiler.h"
#include "Render/Shader/SlangReflectionConverter.h"
#include "Render/Shader/SlangReflectionJson.h"
#include "Render/Shader/ShaderReflection.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <utility>

namespace
{
bool IsSingleShaderStage(const ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
        case ShaderStage::Fragment:
        case ShaderStage::Compute:
            return true;
        default:
            return false;
    }
}

void AssignShaderCompilerError(std::string* errorMessage, std::string message)
{
    if (errorMessage != nullptr)
        *errorMessage = std::move(message);
}

class NullShaderCompiler final : public ShaderCompiler
{
protected:
    ShaderCompileResult CompileProgramImpl(const ShaderCompileRequest&) const override
    {
        ShaderCompileResult result;
        result.m_Succeeded = false;
        result.m_ErrorMessage = "Slang compiler integration has not been implemented yet.";
        return result;
    }
};
} // namespace

bool ValidateShaderCompileRequest(const ShaderCompileRequest& request, std::string* errorMessage)
{
    if (request.m_Source.m_Entries.empty())
    {
        AssignShaderCompilerError(errorMessage, "ShaderCompileRequest requires at least one shader entry point.");
        return false;
    }

    if (request.m_Targets.empty())
    {
        AssignShaderCompilerError(errorMessage, "ShaderCompileRequest requires at least one backend target.");
        return false;
    }

    for (size_t entryIndex = 0; entryIndex < request.m_Source.m_Entries.size(); ++entryIndex)
    {
        const ShaderEntryPointDesc& entry = request.m_Source.m_Entries[entryIndex];
        if (entry.m_ModuleName.empty())
        {
            AssignShaderCompilerError(errorMessage, "Shader entry points require a non-empty module name.");
            return false;
        }

        if (entry.m_EntryName.empty())
        {
            AssignShaderCompilerError(errorMessage, "Shader entry points require a non-empty entry-point name.");
            return false;
        }

        if (!IsSingleShaderStage(entry.m_Stage))
        {
            AssignShaderCompilerError(errorMessage, "Shader entry points must target a single concrete shader stage.");
            return false;
        }

        const auto duplicate =
            std::find_if(request.m_Source.m_Entries.begin(),
                         request.m_Source.m_Entries.begin() + static_cast<std::ptrdiff_t>(entryIndex),
                         [&entry](const ShaderEntryPointDesc& other) { return other.m_Stage == entry.m_Stage; });
        if (duplicate != request.m_Source.m_Entries.begin() + static_cast<std::ptrdiff_t>(entryIndex))
        {
            AssignShaderCompilerError(errorMessage,
                                      "ShaderCompileRequest does not allow duplicate entry points for the same stage.");
            return false;
        }
    }

    for (size_t targetIndex = 0; targetIndex < request.m_Targets.size(); ++targetIndex)
    {
        const ShaderCompileTargetDesc& target = request.m_Targets[targetIndex];
        const auto duplicate = std::find_if(request.m_Targets.begin(),
                                            request.m_Targets.begin() + static_cast<std::ptrdiff_t>(targetIndex),
                                            [&target](const ShaderCompileTargetDesc& other)
                                            { return other.m_Backend == target.m_Backend; });
        if (duplicate != request.m_Targets.begin() + static_cast<std::ptrdiff_t>(targetIndex))
        {
            AssignShaderCompilerError(errorMessage, "ShaderCompileRequest does not allow duplicate backend targets.");
            return false;
        }
    }

    return true;
}

bool ValidateCompiledShaderProgramDesc(const CompiledShaderProgramDesc& program, std::string* errorMessage)
{
    if (program.m_Blobs.empty())
    {
        AssignShaderCompilerError(errorMessage, "CompiledShaderProgramDesc requires at least one backend blob.");
        return false;
    }

    for (size_t blobIndex = 0; blobIndex < program.m_Blobs.size(); ++blobIndex)
    {
        const CompiledShaderBlob& blob = program.m_Blobs[blobIndex];
        if (!IsSingleShaderStage(blob.m_Stage))
        {
            AssignShaderCompilerError(errorMessage, "Compiled shader blobs must carry a single concrete shader stage.");
            return false;
        }

        if (blob.m_EntryPoint.empty())
        {
            AssignShaderCompilerError(errorMessage, "Compiled shader blobs must carry a non-empty entry-point name.");
            return false;
        }

        if (blob.m_Code.empty())
        {
            AssignShaderCompilerError(errorMessage, "Compiled shader blobs must carry backend code bytes.");
            return false;
        }

        const auto duplicate =
            std::find_if(program.m_Blobs.begin(),
                         program.m_Blobs.begin() + static_cast<std::ptrdiff_t>(blobIndex),
                         [&blob](const CompiledShaderBlob& other)
                         { return other.m_Backend == blob.m_Backend && other.m_Stage == blob.m_Stage; });
        if (duplicate != program.m_Blobs.begin() + static_cast<std::ptrdiff_t>(blobIndex))
        {
            std::ostringstream message;
            message << "CompiledShaderProgramDesc contains duplicate blobs for backend "
                    << static_cast<uint32_t>(blob.m_Backend) << " and stage " << static_cast<uint32_t>(blob.m_Stage)
                    << '.';
            AssignShaderCompilerError(errorMessage, message.str());
            return false;
        }
    }

    if (!ValidateShaderReflectionData(program.m_Reflection, errorMessage))
        return false;

    return true;
}

BuildShaderReflectionResult BuildShaderReflectionFromSlangJson(std::string_view jsonText)
{
    BuildShaderReflectionResult result;

    ParseSlangReflectionResult parsedReflection = ParseSlangReflectionJson(jsonText);
    if (!parsedReflection.m_Succeeded)
    {
        result.m_ErrorMessage = std::move(parsedReflection.m_ErrorMessage);
        return result;
    }

    ConvertSlangReflectionResult convertedReflection = ConvertSlangReflectionToNeutral(parsedReflection.m_Document);
    if (!convertedReflection.m_Succeeded)
    {
        result.m_ErrorMessage = std::move(convertedReflection.m_ErrorMessage);
        return result;
    }

    NormalizeShaderReflectionResult normalizedReflection =
        NormalizeShaderReflectionData(std::move(convertedReflection.m_Reflection));
    if (!normalizedReflection.m_Succeeded)
    {
        result.m_ErrorMessage = std::move(normalizedReflection.m_ErrorMessage);
        return result;
    }

    result.m_Succeeded = true;
    result.m_Reflection = std::move(normalizedReflection.m_Reflection);
    return result;
}

bool PopulateProgramReflectionFromSlangJson(CompiledShaderProgramDesc& program,
                                            std::string_view jsonText,
                                            std::string* errorMessage)
{
    BuildShaderReflectionResult reflection = BuildShaderReflectionFromSlangJson(jsonText);
    if (!reflection.m_Succeeded)
    {
        AssignShaderCompilerError(errorMessage, std::move(reflection.m_ErrorMessage));
        return false;
    }

    program.m_Reflection = std::move(reflection.m_Reflection);
    return true;
}

ShaderCompileResult ShaderCompiler::CompileProgram(const ShaderCompileRequest& request) const
{
    ShaderCompileResult result;

    if (!ValidateShaderCompileRequest(request, &result.m_ErrorMessage))
        return result;

    result = CompileProgramImpl(request);
    if (!result.m_Succeeded)
        return result;

    NormalizeShaderReflectionResult normalizedReflection =
        NormalizeShaderReflectionData(std::move(result.m_Program.m_Reflection));
    if (!normalizedReflection.m_Succeeded)
    {
        result.m_Succeeded = false;
        result.m_ErrorMessage = std::move(normalizedReflection.m_ErrorMessage);
        result.m_Program = {};
        return result;
    }

    result.m_Program.m_Reflection = std::move(normalizedReflection.m_Reflection);

    if (!ValidateCompiledShaderProgramDesc(result.m_Program, &result.m_ErrorMessage))
    {
        result.m_Succeeded = false;
        result.m_Program = {};
    }

    return result;
}

Scope<ShaderCompiler> CreateShaderCompiler()
{
    return CreateScope<NullShaderCompiler>();
}
