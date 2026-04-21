#include "Render/Shader/SlangCompiler.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Resource/FileSystem.h"
#include "Core/Resource/IO/PhysicalIO.h"
#include "Render/Shader/SlangReflectionConverter.h"
#include "Render/Shader/SlangReflectionJson.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <system_error>
#include <sstream>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
struct ResolvedSlangExecutable
{
    std::filesystem::path m_Path;
    std::string m_Source;
};

struct SlangCompilerProbeResult
{
    bool m_Succeeded = false;
    std::string m_VersionText;
    std::string m_ErrorMessage;
};

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

std::string GetBackendEntryPointName(const SlangCompileJob& job)
{
    switch (job.m_Backend)
    {
        case BackendType::Vulkan:
            // Slang emits SPIR-V entry points with the exported name "main"
            // even when the source entry function has a different identifier.
            return "main";
        case BackendType::Metal:
        case BackendType::OpenGL:
            return job.m_EntryPoint;
    }

    return job.m_EntryPoint;
}

std::string MakeJobStem(const SlangCompileJob& job, const size_t jobIndex)
{
    std::ostringstream stem;
    stem << "job_" << jobIndex << '_' << GetBackendNameForArtifacts(job.m_Backend) << '_'
         << GetSlangStageName(job.m_Stage);
    return stem.str();
}

std::filesystem::path BuildCompilerSessionDirectory()
{
    const auto sessionStamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return FileSystem::GetCacheDir() / "ShaderCompilerSessions" / std::to_string(sessionStamp);
}

void AppendSessionDirectoryToError(std::string* errorMessage, const std::filesystem::path& sessionDir)
{
    if (errorMessage == nullptr || errorMessage->empty())
        return;

    *errorMessage += " Session artifacts were preserved at: " + sessionDir.generic_string();
}

bool PathLooksExplicit(const std::filesystem::path& path)
{
    return path.is_absolute() || path.has_parent_path();
}

std::optional<std::string> GetEnvironmentVariable(std::string_view name)
{
    if (name.empty())
        return std::nullopt;

    const std::string variableName(name);
    if (const char* value = std::getenv(variableName.c_str()))
        return std::string(value);

    return std::nullopt;
}

void AppendExistingIncludePath(std::vector<std::filesystem::path>* includeSearchPaths, std::filesystem::path path)
{
    if (includeSearchPaths == nullptr || path.empty())
        return;

    if (std::filesystem::exists(path))
        includeSearchPaths->push_back(std::move(path));
}

bool ResolveSlangExecutable(const SlangCompilerConfig& config,
                            ResolvedSlangExecutable* outExecutable,
                            std::string* errorMessage)
{
    if (outExecutable == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "ResolveSlangExecutable requires a valid output destination.");
        return false;
    }

    if (const std::optional<std::string> environmentOverride =
            GetEnvironmentVariable(config.m_ExecutableEnvironmentVariable);
        environmentOverride.has_value() && !environmentOverride->empty())
    {
        outExecutable->m_Path = *environmentOverride;
        outExecutable->m_Source = "environment variable " + config.m_ExecutableEnvironmentVariable;
        return true;
    }

    if (!config.m_ExecutablePath.empty() && PathLooksExplicit(config.m_ExecutablePath))
    {
        if (std::filesystem::exists(config.m_ExecutablePath))
        {
            outExecutable->m_Path = config.m_ExecutablePath;
            outExecutable->m_Source = "explicit SlangCompilerConfig path";
            return true;
        }

        std::ostringstream message;
        message << "Configured slangc executable path does not exist: " << config.m_ExecutablePath.generic_string()
                << '.';
        AssignSlangCompilerPlanningError(errorMessage, message.str());
        return false;
    }

    AssignSlangCompilerPlanningError(errorMessage,
                                     "SlangCompiler requires an explicit slangc path. Configure it through the "
                                     "RTRLAB_SLANGC environment variable or the CMake-injected RTRLAB_SLANGC_PATH.");
    return false;
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

std::string BuildVersionProbeCommandLine(const std::filesystem::path& executablePath)
{
    std::ostringstream command;
    command << QuoteShellArgument(executablePath.generic_string()) << ' ' << QuoteShellArgument("-version");
    return command.str();
}

#if defined(_WIN32)
bool ExecuteWindowsProcess(const std::string& commandLine,
                           const std::filesystem::path& logPath,
                           int* outExitCode,
                           std::string* errorMessage)
{
    if (outExitCode == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "ExecuteWindowsProcess requires a valid exit-code output.");
        return false;
    }

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    const std::string logPathString = logPath.string();
    HANDLE logHandle = CreateFileA(logPathString.c_str(),
                                   GENERIC_WRITE,
                                   FILE_SHARE_READ,
                                   &securityAttributes,
                                   CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
    if (logHandle == INVALID_HANDLE_VALUE)
    {
        std::ostringstream message;
        message << "Failed to create Slang compiler log file: " << logPath.generic_string() << '.';
        AssignSlangCompilerPlanningError(errorMessage, message.str());
        return false;
    }

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = logHandle;
    startupInfo.hStdError = logHandle;

    PROCESS_INFORMATION processInfo{};
    std::string mutableCommandLine = commandLine;
    const BOOL created = CreateProcessA(nullptr,
                                        mutableCommandLine.data(),
                                        nullptr,
                                        nullptr,
                                        TRUE,
                                        CREATE_NO_WINDOW,
                                        nullptr,
                                        nullptr,
                                        &startupInfo,
                                        &processInfo);
    if (!created)
    {
        const DWORD win32Error = GetLastError();
        CloseHandle(logHandle);

        std::ostringstream message;
        message << "Failed to launch Slang compiler process. Win32 error: " << win32Error << '.';
        AssignSlangCompilerPlanningError(errorMessage, message.str());
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD processExitCode = 0;
    if (!GetExitCodeProcess(processInfo.hProcess, &processExitCode))
    {
        const DWORD win32Error = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(logHandle);

        std::ostringstream message;
        message << "Failed to query Slang compiler process exit code. Win32 error: " << win32Error << '.';
        AssignSlangCompilerPlanningError(errorMessage, message.str());
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    CloseHandle(logHandle);

    *outExitCode = static_cast<int>(processExitCode);
    return true;
}
#endif

bool ExecuteCommandLine(const std::string& commandLine,
                        const std::filesystem::path& logPath,
                        int* outExitCode,
                        std::string* errorMessage)
{
#if defined(_WIN32)
    return ExecuteWindowsProcess(commandLine, logPath, outExitCode, errorMessage);
#else
    std::ostringstream redirectedCommand;
    redirectedCommand << commandLine << " > " << QuoteShellArgument(logPath.generic_string()) << " 2>&1";
    if (outExitCode == nullptr)
    {
        AssignSlangCompilerPlanningError(errorMessage, "ExecuteCommandLine requires a valid exit-code output.");
        return false;
    }
    *outExitCode = std::system(redirectedCommand.str().c_str());
    return true;
#endif
}

bool ExecuteCompileCommand(const std::string& commandLine,
                           const std::filesystem::path& logPath,
                           const std::filesystem::path& workingDirectory,
                           const ResolvedSlangExecutable& executable,
                           std::string* errorMessage)
{
    int exitCode = 0;
    if (!ExecuteCommandLine(commandLine, logPath, &exitCode, errorMessage))
        return false;

    if (exitCode == 0)
        return true;

    const std::optional<std::string> compilerOutput = Resource::ReadTextFile(logPath);
    std::ostringstream message;
    message << "slangc invocation failed with exit code " << exitCode
            << ". Executable: " << executable.m_Path.generic_string() << " (" << executable.m_Source
            << "). Working directory: " << workingDirectory.generic_string() << ". Command: " << commandLine;

    if (compilerOutput.has_value() && !compilerOutput->empty())
        message << " Compiler output: " << *compilerOutput;

    AssignSlangCompilerPlanningError(errorMessage, message.str());
    return false;
}

SlangCompilerProbeResult ProbeSlangCompiler(const ResolvedSlangExecutable& executable,
                                            const std::filesystem::path& sessionDir)
{
    SlangCompilerProbeResult result;

    const std::filesystem::path logPath = sessionDir / "slangc_probe.log";
    const std::string commandLine = BuildVersionProbeCommandLine(executable.m_Path);
    int exitCode = 0;
    if (!ExecuteCommandLine(commandLine, logPath, &exitCode, &result.m_ErrorMessage))
        return result;

    const std::optional<std::string> compilerOutput = Resource::ReadTextFile(logPath);

    if (exitCode != 0)
    {
        std::ostringstream message;
        message << "Failed to execute slangc probe. Executable: " << executable.m_Path.generic_string() << " ("
                << executable.m_Source << "). Command: " << commandLine;

        if (compilerOutput.has_value() && !compilerOutput->empty())
            message << " Compiler output: " << *compilerOutput;

        result.m_ErrorMessage = message.str();
        return result;
    }

    result.m_Succeeded = true;
    if (compilerOutput.has_value())
        result.m_VersionText = std::move(*compilerOutput);
    return result;
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
#ifdef RTRLAB_SLANGC_PATH
    config.m_ExecutablePath = RTRLAB_SLANGC_PATH;
#endif
#ifdef RTRLAB_SLANG_STD_MODULE_DIR
    AppendExistingIncludePath(&config.m_IncludeSearchPaths, RTRLAB_SLANG_STD_MODULE_DIR);
#endif
#ifdef RTRLAB_PROJECT_SHADER_DIR
    AppendExistingIncludePath(&config.m_IncludeSearchPaths, RTRLAB_PROJECT_SHADER_DIR);
#endif
#ifdef RTRLAB_PROJECT_SHADER_MODULE_DIR
    AppendExistingIncludePath(&config.m_IncludeSearchPaths, RTRLAB_PROJECT_SHADER_MODULE_DIR);
#endif
    return config;
}

bool ValidateSlangCompilerConfig(const SlangCompilerConfig& config, std::string* errorMessage)
{
    if (config.m_ExecutablePath.empty())
    {
        AssignSlangCompilerPlanningError(errorMessage, "SlangCompiler requires a non-empty slangc executable path.");
        return false;
    }

    for (const std::filesystem::path& includePath : config.m_IncludeSearchPaths)
    {
        if (includePath.empty())
            continue;

        if (!std::filesystem::exists(includePath))
        {
            std::ostringstream message;
            message << "SlangCompiler include search path does not exist: " << includePath.generic_string() << '.';
            AssignSlangCompilerPlanningError(errorMessage, message.str());
            return false;
        }
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

    ResolvedSlangExecutable resolvedExecutable;
    if (!ResolveSlangExecutable(m_Config, &resolvedExecutable, &result.m_ErrorMessage))
        return result;

    SlangCompilerConfig effectiveConfig = m_Config;
    effectiveConfig.m_ExecutablePath = resolvedExecutable.m_Path;
    LOG_INFO_CAT(LogCategory::k_Shader,
                 "Using slangc executable from {}: {}",
                 resolvedExecutable.m_Source,
                 effectiveConfig.m_ExecutablePath.generic_string());

    std::error_code ec;
    const std::filesystem::path sessionDir = BuildCompilerSessionDirectory();
    std::filesystem::create_directories(sessionDir, ec);
    if (ec)
    {
        AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                         "Failed to create the temporary output directory for Slang compilation.");
        return result;
    }

    auto failWithSession = [&](std::string errorMessage) -> ShaderCompileResult
    {
        result.m_Succeeded = false;
        result.m_Program = {};
        result.m_ErrorMessage = std::move(errorMessage);
        AppendSessionDirectoryToError(&result.m_ErrorMessage, sessionDir);
        LOG_ERROR_CAT(LogCategory::k_Shader,
                      "Slang compilation failed. Preserved session artifacts at {}",
                      sessionDir.generic_string());
        return result;
    };

    const SlangCompilerProbeResult probe = ProbeSlangCompiler(resolvedExecutable, sessionDir);
    if (!probe.m_Succeeded)
        return failWithSession(probe.m_ErrorMessage);

    SlangReflectionDocument mergedReflectionDocument;
    bool sawReflection = false;

    LOG_INFO_CAT(LogCategory::k_Shader,
                 "Verified slangc executable from {}: {}",
                 resolvedExecutable.m_Source,
                 effectiveConfig.m_ExecutablePath.generic_string());
    if (!probe.m_VersionText.empty())
        LOG_INFO_CAT(LogCategory::k_Shader, "slangc version: {}", probe.m_VersionText);

    for (size_t jobIndex = 0; jobIndex < plan.m_Jobs.size(); ++jobIndex)
    {
        const SlangCompileJob& job = plan.m_Jobs[jobIndex];
        if (job.m_Backend == BackendType::Metal && job.m_MetalCodeFormat == MetalCodeFormat::Metallib)
        {
            AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                             "SlangCompiler execution does not support Metallib output yet.");
            return failWithSession(result.m_ErrorMessage);
        }

        const std::string stem = MakeJobStem(job, jobIndex);
        const std::filesystem::path outputPath = sessionDir / (stem + GetOutputExtension(job));
        const std::filesystem::path reflectionPath = sessionDir / (stem + ".reflect.json");
        const std::filesystem::path logPath = sessionDir / (stem + ".slangc.log");
        const std::string commandLine = BuildCommandLine(effectiveConfig, job, outputPath, reflectionPath);

        if (!ExecuteCompileCommand(
                commandLine, logPath, std::filesystem::current_path(), resolvedExecutable, &result.m_ErrorMessage))
            return failWithSession(result.m_ErrorMessage);

        std::optional<std::vector<uint8_t>> codeBytes = Resource::ReadBinaryFile(outputPath);
        if (!codeBytes.has_value())
        {
            AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                             "SlangCompiler failed to read a generated backend code artifact.");
            return failWithSession(result.m_ErrorMessage);
        }

        CompiledShaderBlob blob;
        blob.m_Backend = job.m_Backend;
        blob.m_Stage = job.m_Stage;
        blob.m_EntryPoint = GetBackendEntryPointName(job);
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
                return failWithSession(result.m_ErrorMessage);
            }

            ParseSlangReflectionResult parsedReflection = ParseSlangReflectionJson(*reflectionText);
            if (!parsedReflection.m_Succeeded)
                return failWithSession(parsedReflection.m_ErrorMessage);

            if (!MergeReflectionDocument(
                    &mergedReflectionDocument, std::move(parsedReflection.m_Document), &result.m_ErrorMessage))
                return failWithSession(result.m_ErrorMessage);

            sawReflection = true;
        }
    }

    if (!sawReflection)
    {
        AssignSlangCompilerPlanningError(&result.m_ErrorMessage,
                                         "SlangCompiler execution requires reflection JSON output for M4.");
        return failWithSession(result.m_ErrorMessage);
    }

    if (!BuildMergedReflectionData(mergedReflectionDocument, &result.m_Program.m_Reflection, &result.m_ErrorMessage))
        return failWithSession(result.m_ErrorMessage);

    result.m_Succeeded = true;
    LOG_INFO_CAT(LogCategory::k_Shader,
                 "Slang compilation succeeded. Session artifacts are available at {}",
                 sessionDir.generic_string());
    return result;
}

Scope<ShaderCompiler> CreateSlangCompiler(const SlangCompilerConfig& config)
{
    return CreateScope<SlangCompiler>(config);
}
