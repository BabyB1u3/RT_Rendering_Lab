#pragma once

/// @file ShaderReflection.h
/// @brief Shared normalization and validation helpers for neutral shader reflection data.

#include <string>

#include "Render/Shader/ShaderTypes.h"

bool IsBindableReflectedType(ReflectedTypeKind typeKind);
bool IsStructLikeReflectedType(ReflectedTypeKind typeKind);
bool IsPipelineBindableReflectedType(ReflectedTypeKind typeKind);

struct NormalizeShaderReflectionResult
{
    bool m_Succeeded = false;
    ShaderReflectionData m_Reflection;
    std::string m_ErrorMessage;
};

bool ValidateShaderReflectionData(const ShaderReflectionData& reflection, std::string* errorMessage = nullptr);
NormalizeShaderReflectionResult NormalizeShaderReflectionData(ShaderReflectionData reflection);
