#include "Render/Shader/SlangCompiler.h"
#include "Core/Resource/IO/PhysicalIO.h"
#include "Render/Shader/SlangReflectionConverter.h"
#include "Render/Shader/SlangReflectionJson.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <sstream>
#include <unordered_set>
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

std::string GetBackendNameForArtifacts(const BackendType backend)
{
    switch (backend)
    {
        case BackendType::Vulkan:
            return "vulkan";
        case BackendType::Metal:
            return "metal";
        case BackendType::OpenGL:
            return "opengl";
    }

    return "unknown";
}

std::string GetOutputExtension(const SlangCompileJob& job)
{
    switch (job.m_Backend)
    {
        case BackendType::Vulkan:
            return ".spv";
        case BackendType::Metal:
            return job.m_MetalCodeFormat == MetalCodeFormat::Metallib ? ".metallib" : ".metal";
        case BackendType::OpenGL:
            return ".glsl";
    }

    return ".bin";
}

std::string MakeJobStem(const SlangCompileJob& job, const size_t jobIndex)
{
    std::ostringstream stem;
    stem << "job_" << jobIndex << '_' << GetBackendNameForArtifacts(job.m_Backend) << '_'
         << GetSlangStageName(job.m_Stage);
    return stem.str();
}

#if defined(_WIN32)
std::string QuoteShellArgument(std::string_view argument)
{
    std::string quoted;
    quoted.push_back('"');
    for (const char ch : argument)
    {
        if (ch == '"' || ch == '\\')
            quoted.push_back('\\');
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}
#else
std::string QuoteShellArgument(std::string_view argument)
{
    std::string quoted;
    quoted.push_back('\'');
    for (const char ch : argument)
    {
        if (ch == '\'')
            quoted.append("'\"'\"'");
        else
            quoted.push_back(ch);
    }
    quoted.push_back('\'');
    return quoted;
}
#endif

std::string BuildCommandLine(const SlangCompilerConfig& config,
                             const SlangCompileJob& job,
                             const std::filesystem::path& outputPath,
                             const std::filesystem::path& reflectionPath)
{
    std::ostringstream command;
    command << QuoteShellArgument(config.m_ExecutablePath.generic_string());

    for (const std::string& argument : job.m_Arguments)
    {
        command << ' ';
        if (argument == "-reflection-json")
            command << QuoteShellArgument(argument) << ' ' << QuoteShellArgument(reflectionPath.generic_string());
        else
            command << QuoteShellArgument(argument);
    }

    command << ' ' << QuoteShellArgument("-o") << ' ' << QuoteShellArgument(outputPath.generic_string());
    return command.str();
}

bool ExecuteCompileCommand(const std::string& commandLine, std::string* errorMessage)
{
    const int exitCode = std::system(commandLine.c_str());
    if (exitCode == 0)
        return true;

    std::ostringstream message;
    message << "slangc invocation failed with exit code " << exitCode << '.';
    AssignSlangCompilerPlanningError(errorMessage, message.str());
    return false;
}

bool MergeReflectionDocument(SlangReflectionDocument* destination,
                             SlangReflectionDocument source,
                             std::string* errorMessage)
{
    if (destination == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "Reflection merge requires a valid destination document.");
        return false;
    }

    std::unordered_set<std::string> knownParameters;
    knownParameters.reserve(destination->m_Parameters.size() + source.m_Parameters.size());
    for (const SlangReflectionParameter& parameter : destination->m_Parameters)
        knownParameters.insert(parameter.m_Name);

    for (SlangReflectionParameter& parameter : source.m_Parameters)
    {
        if (knownParameters.insert(parameter.m_Name).second)
            destination->m_Parameters.push_back(std::move(parameter));
    }

    for (SlangReflectionEntryPoint& entryPoint : source.m_EntryPoints)
        destination->m_EntryPoints.push_back(std::move(entryPoint));

    return true;
}

bool BuildMergedReflectionData(const SlangReflectionDocument& document,
                               ShaderReflectionData* outReflection,
                               std::string* errorMessage)
{
    if (outReflection == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "Merged reflection conversion requires an output object.");
        return false;
    }

    ConvertSlangReflectionResult converted = ConvertSlangReflectionToNeutral(document);
    if (!converted.m_Succeeded)
    {
        AssignSlangCompilerPlanningError(errorMessage, std::move(converted.m_ErrorMessage));
        return false;
    }

    *outReflection = std::move(converted.m_Reflection);
    return true;
}

bool AppendJobArguments(const ShaderCompileTargetDesc& target,
                        const ShaderSourceDesc& source,
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

    for (const std::string& define : source.m_Defines)
    {
        outJob->m_Arguments.push_back("-D");
        outJob->m_Arguments.push_back(define);
    }

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

SlangCompilerConfig CreateDefaultSlangCompilerConfig()
{
    SlangCompilerConfig config;
    config.m_ExecutablePath = "slangc";
    return config;
}

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
            if (!AppendJobArguments(target, request.m_Source, entry, config, &job, errorMessage))
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

    std::error_code ec;
    const auto sessionStamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path tempRoot = std::filesystem::temp_directory_path(ec);
    const std::filesystem::path sessionDir =
        (ec ? std::filesystem::current_path() : tempRoot) / "RTRLabSlangCompiler" / std::to_string(sessionStamp);
    std::filesystem::create_directories(sessionDir, ec);
    if (ec)
    {
        AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                         "Failed to create the temporary output directory for Slang compilation.");
        return result;
    }

    SlangReflectionDocument mergedReflectionDocument;
    bool sawReflection = false;

    for (size_t jobIndex = 0; jobIndex < plan.m_Jobs.size(); ++jobIndex)
    {
        const SlangCompileJob& job = plan.m_Jobs[jobIndex];
        if (job.m_Backend == BackendType::Metal && job.m_MetalCodeFormat == MetalCodeFormat::Metallib)
        {
            AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                             "SlangCompiler execution does not support Metallib output yet.");
            result.m_Program = {};
            return result;
        }

        const std::string stem = MakeJobStem(job, jobIndex);
        const std::filesystem::path outputPath = sessionDir / (stem + GetOutputExtension(job));
        const std::filesystem::path reflectionPath = sessionDir / (stem + ".reflect.json");
        const std::string commandLine = BuildCommandLine(m_Config, job, outputPath, reflectionPath);

        if (!ExecuteCompileCommand(commandLine, &result.m_ErrorMessage))
        {
            result.m_Program = {};
            return result;
        }

        std::optional<std::vector<uint8_t>> codeBytes = Resource::ReadBinaryFile(outputPath);
        if (!codeBytes.has_value())
        {
            AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                             "SlangCompiler failed to read a generated backend code artifact.");
            result.m_Program = {};
            return result;
        }

        CompiledShaderBlob blob;
        blob.m_Backend = job.m_Backend;
        blob.m_Stage = job.m_Stage;
        blob.m_EntryPoint = job.m_EntryPoint;
        blob.m_MetalCodeFormat = job.m_MetalCodeFormat;
        blob.m_Code = std::move(*codeBytes);
        result.m_Program.m_Blobs.push_back(std::move(blob));

        if (job.m_EmitReflectionJson)
        {
            std::optional<std::string> reflectionText = Resource::ReadTextFile(reflectionPath);
            if (!reflectionText.has_value())
            {
                AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                                 "SlangCompiler failed to read a generated reflection sidecar.");
                result.m_Program = {};
                return result;
            }

            ParseSlangReflectionResult parsedReflection = ParseSlangReflectionJson(*reflectionText);
            if (!parsedReflection.m_Succeeded)
            {
                AssignSlangCompilerPlanningError(&result.m_ErrorMessage, std::move(parsedReflection.m_ErrorMessage));
                result.m_Program = {};
                return result;
            }

            if (!MergeReflectionDocument(
                    &mergedReflectionDocument, std::move(parsedReflection.m_Document), &result.m_ErrorMessage))
            {
                result.m_Program = {};
                return result;
            }

            sawReflection = true;
        }
    }

    if (!sawReflection)
    {
        AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                         "SlangCompiler execution requires reflection JSON output for M4.");
        result.m_Program = {};
        return result;
    }

    if (!BuildMergedReflectionData(mergedReflectionDocument, &result.m_Program.m_Reflection, &result.m_ErrorMessage))
    {
        result.m_Program = {};
        return result;
    }

    result.m_Succeeded = true;
    return result;
}

Scope<ShaderCompiler> CreateSlangCompiler(const SlangCompilerConfig& config)
{
    return CreateScope<SlangCompiler>(config);
}
