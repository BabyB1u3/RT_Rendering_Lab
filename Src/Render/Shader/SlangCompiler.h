#pragma once

/// @file SlangCompiler.h
/// @brief Slang compiler configuration and compile-plan planning helpers.

#include <filesystem>
#include <string>
#include <vector>

#include "Render/Shader/ShaderCompiler.h"

struct SlangCompilerConfig
{
    std::filesystem::path m_ExecutablePath;
    std::vector<std::filesystem::path> m_IncludeSearchPaths;
    bool m_EmitReflectionJson = true;
    bool m_UseColumnMajorMatrices = true;
};

struct SlangCompileJob
{
    BackendType m_Backend = BackendType::Vulkan;
    ShaderStage m_Stage = ShaderStage::None;
    std::string m_ModuleName;
    std::string m_EntryPoint;
    std::string m_Target;
    std::string m_Profile;
    std::vector<std::string> m_Arguments;
    bool m_EmitReflectionJson = false;
    bool m_EmitsBinaryCode = false;
    MetalCodeFormat m_MetalCodeFormat = MetalCodeFormat::MslSource;
};

struct SlangCompilePlan
{
    std::vector<SlangCompileJob> m_Jobs;
};

SlangCompilerConfig CreateDefaultSlangCompilerConfig();
bool ValidateSlangCompilerConfig(const SlangCompilerConfig& config, std::string* errorMessage = nullptr);
bool BuildSlangCompilePlan(const ShaderCompileRequest& request,
                           const SlangCompilerConfig& config,
                           SlangCompilePlan* outPlan,
                           std::string* errorMessage = nullptr);

class SlangCompiler final : public ShaderCompiler
{
public:
    explicit SlangCompiler(SlangCompilerConfig config);

    const SlangCompilerConfig& GetConfig() const { return m_Config; }
    bool BuildCompilePlan(const ShaderCompileRequest& request,
                          SlangCompilePlan* outPlan,
                          std::string* errorMessage = nullptr) const;

protected:
    ShaderCompileResult CompileProgramImpl(const ShaderCompileRequest& request) const override;

private:
    SlangCompilerConfig m_Config;
};

Scope<ShaderCompiler> CreateSlangCompiler(const SlangCompilerConfig& config);
