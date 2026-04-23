#pragma once

/// @file SlangReflectionConverter.h
/// @brief Converts raw Slang reflection JSON structures into neutral engine reflection data.

#include <string>

#include "Render/Shader/ShaderTypes.h"
#include "Render/Shader/SlangReflectionJson.h"

struct ConvertSlangReflectionResult
{
    bool m_Succeeded = false;
    ShaderReflectionData m_Reflection;
    std::string m_ErrorMessage;
};

ConvertSlangReflectionResult ConvertSlangReflectionToNeutral(const SlangReflectionDocument& document);
