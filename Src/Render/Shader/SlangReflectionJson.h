#pragma once

/// @file SlangReflectionJson.h
/// @brief Shared parser for raw `slangc -reflection-json` documents.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct SlangReflectionBinding
{
    std::string m_Kind;
    uint32_t m_Index = 0;
    uint32_t m_Space = 0;
    bool m_HasExplicitSpace = false;
    uint32_t m_Offset = 0;
    uint32_t m_Size = 0;
};

struct SlangReflectionType;

struct SlangReflectionField
{
    std::string m_Name;
    std::unique_ptr<SlangReflectionType> m_Type;
    std::optional<SlangReflectionBinding> m_Binding;
};

struct SlangReflectionType
{
    std::string m_Kind;
    std::string m_Name;
    std::string m_ScalarType;

    uint32_t m_ElementCount = 0;
    uint32_t m_RowCount = 0;
    uint32_t m_ColumnCount = 0;

    std::unique_ptr<SlangReflectionType> m_ElementType;
    std::vector<SlangReflectionField> m_Fields;
    std::vector<SlangReflectionType> m_GenericArguments;
};

struct SlangReflectionParameter
{
    std::string m_Name;
    SlangReflectionType m_Type;
    std::optional<SlangReflectionBinding> m_Binding;
};

struct SlangReflectionEntryPointBinding
{
    std::string m_Name;
    SlangReflectionBinding m_Binding;
};

struct SlangReflectionEntryPoint
{
    std::string m_Name;
    std::string m_Stage;
    std::vector<SlangReflectionEntryPointBinding> m_Bindings;
};

struct SlangReflectionDocument
{
    std::vector<SlangReflectionParameter> m_Parameters;
    std::vector<SlangReflectionEntryPoint> m_EntryPoints;
};

struct ParseSlangReflectionResult
{
    bool m_Succeeded = false;
    SlangReflectionDocument m_Document;
    std::string m_ErrorMessage;
};

ParseSlangReflectionResult ParseSlangReflectionJson(std::string_view jsonText);
