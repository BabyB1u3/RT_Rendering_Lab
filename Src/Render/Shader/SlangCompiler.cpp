#include "Render/Shader/SlangCompiler.h"

#include <sstream>
#include <utility>

namespace
{
void AssignSlangCompilerPlanningError(std::string* errorMessage, std::string message)
{
    if (errorMessage != nullptr)
        *errorMessage = std::move(message);
}

bool IsConcreteShaderStage(const ShaderStage stage)
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

std::string GetSlangTarget(const BackendType backend)
{
    switch (backend)
    {
        case BackendType::Vulkan:
            return "spirv";
        case BackendType::Metal:
            return "metal";
        case BackendType::OpenGL:
            return "glsl";
    }

    return {};
}

std::string GetSlangProfile(const BackendType backend)
{
    switch (backend)
    {
        case BackendType::OpenGL:
            return "glsl_460";
        case BackendType::Vulkan:
        case BackendType::Metal:
            return {};
    }

    return {};
}

std::string GetSlangStageName(const ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return "vertex";
        case ShaderStage::Fragment:
            return "fragment";
        case ShaderStage::Compute:
            return "compute";
        default:
            return {};
    }
}

bool AppendJobArguments(const ShaderCompileTargetDesc& target,
                        const ShaderEntryPointDesc& entry,
                        const SlangCompilerConfig& config,
                        SlangCompileJob* outJob,
                        std::string* errorMessage)
{
    if (outJob == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "Slang compile jobs require a valid output destination.");
        return false;
    }

    outJob->m_Backend = target.m_Backend;
    outJob->m_Stage = entry.m_Stage;
    outJob->m_ModuleName = entry.m_ModuleName;
    outJob->m_EntryPoint = entry.m_EntryName;
    outJob->m_Target = GetSlangTarget(target.m_Backend);
    outJob->m_Profile = GetSlangProfile(target.m_Backend);
    outJob->m_EmitReflectionJson = config.m_EmitReflectionJson;
    outJob->m_EmitsBinaryCode = target.m_Backend == BackendType::Vulkan;
    outJob->m_MetalCodeFormat = target.m_MetalCodeFormat;

    if (outJob->m_Target.empty())
    {
        AssignSlangCompilerPlanningError(errorMessage, "Unsupported backend target for Slang compile planning.");
        return false;
    }

    const std::string stageName = GetSlangStageName(entry.m_Stage);
    if (stageName.empty())
    {
        AssignSlangCompilerPlanningError(errorMessage, "Slang compile planning requires a concrete shader stage.");
        return false;
    }

    outJob->m_Arguments.push_back(entry.m_ModuleName);
    outJob->m_Arguments.push_back("-target");
    outJob->m_Arguments.push_back(outJob->m_Target);

    if (!outJob->m_Profile.empty())
    {
        outJob->m_Arguments.push_back("-profile");
        outJob->m_Arguments.push_back(outJob->m_Profile);
    }

    if (target.m_Backend == BackendType::Vulkan)
        outJob->m_Arguments.push_back("-emit-spirv-directly");

    if (config.m_UseColumnMajorMatrices)
        outJob->m_Arguments.push_back("-matrix-layout-column-major");

    outJob->m_Arguments.push_back("-stage");
    outJob->m_Arguments.push_back(stageName);
    outJob->m_Arguments.push_back("-entry");
    outJob->m_Arguments.push_back(entry.m_EntryName);

    for (const std::filesystem::path& includePath : config.m_IncludeSearchPaths)
    {
        outJob->m_Arguments.push_back("-I");
        outJob->m_Arguments.push_back(includePath.generic_string());
    }

    if (config.m_EmitReflectionJson)
        outJob->m_Arguments.push_back("-reflection-json");

    return true;
}
} // namespace

bool ValidateSlangCompilerConfig(const SlangCompilerConfig& config, std::string* errorMessage)
{
    if (config.m_ExecutablePath.empty())
    {
        AssignSlangCompilerPlanningError(errorMessage, "SlangCompiler requires a non-empty slangc executable path.");
        return false;
    }

    return true;
}

bool BuildSlangCompilePlan(const ShaderCompileRequest& request,
                           const SlangCompilerConfig& config,
                           SlangCompilePlan* outPlan,
                           std::string* errorMessage)
{
    if (outPlan == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "BuildSlangCompilePlan requires a valid output plan.");
        return false;
    }

    if (!ValidateSlangCompilerConfig(config, errorMessage))
        return false;

    outPlan->m_Jobs.clear();
    outPlan->m_Jobs.reserve(request.m_Source.m_Entries.size() * request.m_Targets.size());

    for (const ShaderCompileTargetDesc& target : request.m_Targets)
    {
        for (const ShaderEntryPointDesc& entry : request.m_Source.m_Entries)
        {
            if (!IsConcreteShaderStage(entry.m_Stage))
            {
                AssignSlangCompilerPlanningError(errorMessage,
                                                 "Slang compile planning only supports concrete entry-point stages.");
                outPlan->m_Jobs.clear();
                return false;
            }

            SlangCompileJob job;
            if (!AppendJobArguments(target, entry, config, &job, errorMessage))
            {
                outPlan->m_Jobs.clear();
                return false;
            }

            outPlan->m_Jobs.push_back(std::move(job));
        }
    }

    return true;
}

SlangCompiler::SlangCompiler(SlangCompilerConfig config) : m_Config(std::move(config)) {}

bool SlangCompiler::BuildCompilePlan(const ShaderCompileRequest& request,
                                     SlangCompilePlan* outPlan,
                                     std::string* errorMessage) const
{
    return BuildSlangCompilePlan(request, m_Config, outPlan, errorMessage);
}

ShaderCompileResult SlangCompiler::CompileProgramImpl(const ShaderCompileRequest& request) const
{
    ShaderCompileResult result;

    SlangCompilePlan plan;
    if (!BuildCompilePlan(request, &plan, &result.m_ErrorMessage))
        return result;

    std::ostringstream message;
    message << "Slang compile execution is not implemented yet. Built " << plan.m_Jobs.size()
            << " compile job(s) successfully.";
    result.m_ErrorMessage = message.str();
    return result;
}

Scope<ShaderCompiler> CreateSlangCompiler(const SlangCompilerConfig& config)
{
    return CreateScope<SlangCompiler>(config);
}
