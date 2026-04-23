#include "Render/Shader/SlangReflectionConverter.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
void AssignSlangReflectionConverterError(std::string* errorMessage, std::string message)
{
    if (errorMessage != nullptr)
        *errorMessage = std::move(message);
}

ShaderStage ParseSlangStage(std::string_view stage)
{
    if (stage == "vertex")
        return ShaderStage::Vertex;
    if (stage == "fragment")
        return ShaderStage::Fragment;
    if (stage == "compute")
        return ShaderStage::Compute;
    return ShaderStage::None;
}

ShaderStage CollectDocumentStageMask(const SlangReflectionDocument& document)
{
    ShaderStage stageMask = ShaderStage::None;
    for (const SlangReflectionEntryPoint& entryPoint : document.m_EntryPoints)
        stageMask |= ParseSlangStage(entryPoint.m_Stage);
    return stageMask;
}

std::unordered_map<std::string, ShaderStage> BuildParameterStageMaskTable(const SlangReflectionDocument& document)
{
    std::unordered_map<std::string, ShaderStage> stageMasks;
    for (const SlangReflectionEntryPoint& entryPoint : document.m_EntryPoints)
    {
        const ShaderStage stage = ParseSlangStage(entryPoint.m_Stage);
        if (stage == ShaderStage::None)
            continue;

        for (const SlangReflectionEntryPointBinding& binding : entryPoint.m_Bindings)
            stageMasks[binding.m_Name] |= stage;
    }

    return stageMasks;
}

const SlangReflectionType* ResolveStructuredElementType(const SlangReflectionType& type)
{
    if (type.m_ElementType != nullptr)
        return type.m_ElementType.get();
    if (!type.m_GenericArguments.empty())
        return &type.m_GenericArguments.front();
    return &type;
}

bool IsStructLikeKind(std::string_view kind)
{
    return kind == "struct" || kind == "constantBuffer" || kind == "parameterBlock";
}

bool IsLeafConstantType(const SlangReflectionType& type)
{
    const std::string_view kind = type.m_Kind;
    return kind == "scalar" || kind == "vector" || kind == "matrix";
}

const SlangReflectionType& ResolveBindableType(const SlangReflectionType& type)
{
    if (type.m_Kind == "array" && type.m_ElementType != nullptr)
        return ResolveBindableType(*type.m_ElementType);
    return type;
}

uint32_t GetBindableArrayCount(const SlangReflectionType& type)
{
    if (type.m_Kind == "array")
    {
        const uint32_t elementCount = std::max(type.m_ElementCount, 1u);
        if (type.m_ElementType != nullptr)
            return elementCount * GetBindableArrayCount(*type.m_ElementType);
        return elementCount;
    }

    return 1;
}

uint32_t GetParameterBlockByteSize(const SlangReflectionType& type, const SlangReflectionBinding& binding)
{
    if (type.m_ElementVarBinding.has_value() && type.m_ElementVarBinding->m_Size > 0)
        return type.m_ElementVarBinding->m_Size;
    return binding.m_Size;
}

ReflectedTypeKind MapResourceTypeKind(const SlangReflectionType& type, const SlangReflectionBinding& binding)
{
    const std::string_view bindingKind = binding.m_Kind;
    const SlangReflectionType& bindableType = ResolveBindableType(type);
    const std::string_view typeKind = bindableType.m_Kind;
    const std::string_view typeName = bindableType.m_Name;

    if (bindingKind == "samplerState" || typeKind == "samplerState")
        return ReflectedTypeKind::Sampler;

    if (bindingKind == "constantBuffer")
        return ReflectedTypeKind::ParameterBlock;

    if (bindingKind == "shaderResource" || bindingKind == "unorderedAccess" || bindingKind == "descriptorTableSlot")
    {
        if (typeName.find("Buffer") != std::string::npos || typeKind == "shaderStorageBuffer" ||
            typeKind == "textureBuffer")
        {
            return ReflectedTypeKind::Buffer;
        }

        if (bindingKind == "unorderedAccess")
            return ReflectedTypeKind::StorageTexture;

        return ReflectedTypeKind::Texture;
    }

    if (typeKind == "resource")
        return ReflectedTypeKind::Texture;

    return ReflectedTypeKind::Texture;
}

bool ConvertFieldChildren(const SlangReflectionType& type,
                          ShaderStage stageMask,
                          std::vector<ReflectedField>& outChildren,
                          std::string* errorMessage);

bool ConvertStructuredField(const SlangReflectionField& field,
                            ShaderStage stageMask,
                            ReflectedField& outField,
                            std::string* errorMessage)
{
    if (field.m_Type == nullptr)
    {
        AssignSlangReflectionConverterError(errorMessage, "Structured Slang reflection fields require a type payload.");
        return false;
    }

    outField.m_Name = field.m_Name;
    outField.m_StageMask = stageMask;
    outField.m_LayoutConvention = LayoutConvention::Std430;

    const SlangReflectionBinding binding = field.m_Binding.value_or(SlangReflectionBinding{});

    if (field.m_Type->m_Kind == "parameterBlock")
    {
        outField.m_TypeKind = ReflectedTypeKind::ParameterBlock;
        outField.m_SetIndex = binding.m_Space;
        outField.m_Binding = binding.m_Index;
        outField.m_Size = GetParameterBlockByteSize(*field.m_Type, binding);
        outField.m_ArrayCount = GetBindableArrayCount(*field.m_Type);
        return ConvertFieldChildren(
            *ResolveStructuredElementType(*field.m_Type), stageMask, outField.m_Children, errorMessage);
    }

    if (field.m_Binding.has_value() && (binding.m_Kind == "constantBuffer" || field.m_Type->m_Kind == "constantBuffer"))
    {
        outField.m_TypeKind = ReflectedTypeKind::ParameterBlock;
        outField.m_SetIndex = binding.m_Space;
        outField.m_Binding = binding.m_Index;
        outField.m_Size = GetParameterBlockByteSize(*field.m_Type, binding);
        outField.m_ArrayCount = GetBindableArrayCount(*field.m_Type);
        return ConvertFieldChildren(
            *ResolveStructuredElementType(*field.m_Type), stageMask, outField.m_Children, errorMessage);
    }

    if (IsLeafConstantType(*field.m_Type))
    {
        outField.m_TypeKind = ReflectedTypeKind::ConstantData;
        outField.m_Offset = binding.m_Offset;
        outField.m_Size = binding.m_Size;
        return true;
    }

    if (field.m_Binding.has_value() && !IsStructLikeKind(field.m_Type->m_Kind))
    {
        outField.m_TypeKind = MapResourceTypeKind(*field.m_Type, binding);
        outField.m_SetIndex = binding.m_Space;
        outField.m_Binding = binding.m_Index;
        outField.m_ArrayCount = GetBindableArrayCount(*field.m_Type);
        return true;
    }

    if (IsStructLikeKind(field.m_Type->m_Kind))
    {
        outField.m_TypeKind = ReflectedTypeKind::Struct;
        outField.m_Offset = binding.m_Offset;
        outField.m_Size = binding.m_Size;
        return ConvertFieldChildren(
            *ResolveStructuredElementType(*field.m_Type), stageMask, outField.m_Children, errorMessage);
    }

    AssignSlangReflectionConverterError(
        errorMessage, "Unsupported Slang reflection field kind encountered during neutral conversion.");
    return false;
}

bool ConvertFieldChildren(const SlangReflectionType& type,
                          ShaderStage stageMask,
                          std::vector<ReflectedField>& outChildren,
                          std::string* errorMessage)
{
    outChildren.reserve(type.m_Fields.size());
    for (const SlangReflectionField& child : type.m_Fields)
    {
        ReflectedField reflectedChild;
        if (!ConvertStructuredField(child, stageMask, reflectedChild, errorMessage))
            return false;
        outChildren.push_back(std::move(reflectedChild));
    }

    return true;
}

bool ConvertParameter(const SlangReflectionParameter& parameter,
                      ShaderStage stageMask,
                      ReflectedField& outField,
                      std::string* errorMessage)
{
    outField.m_Name = parameter.m_Name;
    outField.m_StageMask = stageMask;
    outField.m_LayoutConvention = LayoutConvention::Std430;

    const SlangReflectionBinding binding = parameter.m_Binding.value_or(SlangReflectionBinding{});
    const std::string_view typeKind = parameter.m_Type.m_Kind;

    if (typeKind == "parameterBlock" || typeKind == "constantBuffer" || binding.m_Kind == "constantBuffer")
    {
        outField.m_TypeKind = ReflectedTypeKind::ParameterBlock;
        outField.m_SetIndex = binding.m_Space;
        outField.m_Binding = binding.m_Index;
        outField.m_Size = GetParameterBlockByteSize(parameter.m_Type, binding);
        outField.m_ArrayCount = GetBindableArrayCount(parameter.m_Type);
        return ConvertFieldChildren(
            *ResolveStructuredElementType(parameter.m_Type), stageMask, outField.m_Children, errorMessage);
    }

    if (parameter.m_Binding.has_value())
    {
        outField.m_TypeKind = MapResourceTypeKind(parameter.m_Type, binding);
        outField.m_SetIndex = binding.m_Space;
        outField.m_Binding = binding.m_Index;
        outField.m_ArrayCount = GetBindableArrayCount(parameter.m_Type);
        return true;
    }

    AssignSlangReflectionConverterError(
        errorMessage, "Top-level Slang reflection parameters must be bindable resources or parameter blocks.");
    return false;
}
} // namespace

ConvertSlangReflectionResult ConvertSlangReflectionToNeutral(const SlangReflectionDocument& document)
{
    ConvertSlangReflectionResult result;

    const ShaderStage documentStageMask = CollectDocumentStageMask(document);
    const std::unordered_map<std::string, ShaderStage> parameterStageMasks = BuildParameterStageMaskTable(document);

    result.m_Reflection.m_Globals.reserve(document.m_Parameters.size());
    for (const SlangReflectionParameter& parameter : document.m_Parameters)
    {
        ReflectedField reflectedParameter;
        const auto stageMaskIt = parameterStageMasks.find(parameter.m_Name);
        const ShaderStage stageMask =
            stageMaskIt != parameterStageMasks.end() ? stageMaskIt->second : documentStageMask;

        if (!ConvertParameter(parameter, stageMask, reflectedParameter, &result.m_ErrorMessage))
            return result;

        result.m_Reflection.m_Globals.push_back(std::move(reflectedParameter));
    }

    result.m_Succeeded = true;
    return result;
}
