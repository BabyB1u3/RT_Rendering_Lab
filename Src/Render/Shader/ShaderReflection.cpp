#include "Render/Shader/ShaderReflection.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace
{
void AssignError(std::string* errorMessage, std::string message)
{
    if (errorMessage != nullptr)
        *errorMessage = std::move(message);
}

uint32_t GetFieldSortPriority(const ReflectedField& field)
{
    switch (field.m_TypeKind)
    {
        case ReflectedTypeKind::ParameterBlock:
            return 0;
        case ReflectedTypeKind::ConstantData:
            return 1;
        case ReflectedTypeKind::Struct:
            return 2;
        case ReflectedTypeKind::Buffer:
            return 3;
        case ReflectedTypeKind::Texture:
            return 4;
        case ReflectedTypeKind::Sampler:
            return 5;
    }

    return 6;
}

bool CompareReflectedFields(const ReflectedField& lhs, const ReflectedField& rhs)
{
    const uint32_t lhsPriority = GetFieldSortPriority(lhs);
    const uint32_t rhsPriority = GetFieldSortPriority(rhs);
    if (lhsPriority != rhsPriority)
        return lhsPriority < rhsPriority;

    const bool lhsBindable = IsPipelineBindableReflectedType(lhs.m_TypeKind);
    const bool rhsBindable = IsPipelineBindableReflectedType(rhs.m_TypeKind);
    if (lhsBindable && rhsBindable)
    {
        if (lhs.m_SetIndex != rhs.m_SetIndex)
            return lhs.m_SetIndex < rhs.m_SetIndex;
        if (lhs.m_Binding != rhs.m_Binding)
            return lhs.m_Binding < rhs.m_Binding;
    }

    const bool lhsStructLike = IsStructLikeReflectedType(lhs.m_TypeKind);
    const bool rhsStructLike = IsStructLikeReflectedType(rhs.m_TypeKind);
    if (lhsStructLike && rhsStructLike && lhs.m_Offset != rhs.m_Offset)
        return lhs.m_Offset < rhs.m_Offset;

    return lhs.m_Name < rhs.m_Name;
}

struct NormalizeContext
{
    bool m_InsideParameterBlock = false;
    uint32_t m_SetIndex = 0;
    ShaderStage m_StageMask = ShaderStage::None;
};

void NormalizeReflectedField(ReflectedField& field, const NormalizeContext& context)
{
    if (IsPipelineBindableReflectedType(field.m_TypeKind))
        field.m_ArrayCount = std::max(field.m_ArrayCount, 1u);

    if (context.m_InsideParameterBlock)
        field.m_SetIndex = context.m_SetIndex;

    if (field.m_StageMask == ShaderStage::None)
        field.m_StageMask = context.m_StageMask;

    NormalizeContext childContext = context;
    if (field.m_TypeKind == ReflectedTypeKind::ParameterBlock)
    {
        childContext.m_InsideParameterBlock = true;
        childContext.m_SetIndex = field.m_SetIndex;
        childContext.m_StageMask = field.m_StageMask;
    }

    for (ReflectedField& child : field.m_Children)
        NormalizeReflectedField(child, childContext);

    std::sort(field.m_Children.begin(), field.m_Children.end(), CompareReflectedFields);
}

bool ValidateReflectedField(const ReflectedField& field,
                            const bool insideParameterBlock,
                            std::string_view parentPath,
                            std::string* errorMessage)
{
    if (field.m_Name.empty())
    {
        AssignError(errorMessage, "Reflected fields require a non-empty name.");
        return false;
    }

    if (IsStructLikeReflectedType(field.m_TypeKind) && field.m_LayoutConvention != LayoutConvention::Std430)
    {
        AssignError(errorMessage, "v1 shader reflection requires Std430 layout for all struct/constant data fields.");
        return false;
    }

    if (IsPipelineBindableReflectedType(field.m_TypeKind) && field.m_ArrayCount == 0)
    {
        AssignError(errorMessage, "Reflected resource bindings require an array count of at least 1.");
        return false;
    }

    if (insideParameterBlock && field.m_TypeKind == ReflectedTypeKind::ParameterBlock)
    {
        AssignError(errorMessage, "v1 shader reflection does not support nested ParameterBlock fields.");
        return false;
    }

    std::unordered_set<std::string> childNames;
    childNames.reserve(field.m_Children.size());
    for (const ReflectedField& child : field.m_Children)
    {
        if (!childNames.insert(child.m_Name).second)
        {
            AssignError(errorMessage, "Reflected sibling fields must have unique names for path-based lookup.");
            return false;
        }

        if (!ValidateReflectedField(child,
                                    insideParameterBlock || field.m_TypeKind == ReflectedTypeKind::ParameterBlock,
                                    parentPath.empty() ? field.m_Name : std::string(parentPath) + "." + field.m_Name,
                                    errorMessage))
        {
            return false;
        }
    }

    return true;
}
} // namespace

bool IsBindableReflectedType(ReflectedTypeKind typeKind)
{
    return typeKind == ReflectedTypeKind::Texture || typeKind == ReflectedTypeKind::Sampler ||
           typeKind == ReflectedTypeKind::Buffer;
}

bool IsStructLikeReflectedType(ReflectedTypeKind typeKind)
{
    return typeKind == ReflectedTypeKind::Struct || typeKind == ReflectedTypeKind::ConstantData;
}

bool IsPipelineBindableReflectedType(ReflectedTypeKind typeKind)
{
    return IsBindableReflectedType(typeKind) || typeKind == ReflectedTypeKind::ParameterBlock;
}

bool ValidateShaderReflectionData(const ShaderReflectionData& reflection, std::string* errorMessage)
{
    std::unordered_set<std::string> globalNames;
    globalNames.reserve(reflection.m_Globals.size());

    for (const ReflectedField& field : reflection.m_Globals)
    {
        if (!globalNames.insert(field.m_Name).second)
        {
            AssignError(errorMessage, "Shader reflection globals must have unique names.");
            return false;
        }

        if (!ValidateReflectedField(field, false, "", errorMessage))
            return false;
    }

    for (const PushConstantRangeDesc& pushConstant : reflection.m_PushConstants)
    {
        if (pushConstant.m_Size == 0)
        {
            AssignError(errorMessage, "Push constant ranges must have a non-zero size.");
            return false;
        }

        if (pushConstant.m_StageMask == ShaderStage::None)
        {
            AssignError(errorMessage, "Push constant ranges must target at least one shader stage.");
            return false;
        }
    }

    return true;
}

NormalizeShaderReflectionResult NormalizeShaderReflectionData(ShaderReflectionData reflection)
{
    for (ReflectedField& field : reflection.m_Globals)
        NormalizeReflectedField(field, {});

    std::sort(reflection.m_Globals.begin(), reflection.m_Globals.end(), CompareReflectedFields);
    std::sort(reflection.m_PushConstants.begin(),
              reflection.m_PushConstants.end(),
              [](const PushConstantRangeDesc& lhs, const PushConstantRangeDesc& rhs)
              {
                  if (lhs.m_Offset != rhs.m_Offset)
                      return lhs.m_Offset < rhs.m_Offset;
                  if (lhs.m_Size != rhs.m_Size)
                      return lhs.m_Size < rhs.m_Size;
                  return static_cast<uint32_t>(lhs.m_StageMask) < static_cast<uint32_t>(rhs.m_StageMask);
              });

    NormalizeShaderReflectionResult result;
    result.m_Reflection = std::move(reflection);
    result.m_Succeeded = ValidateShaderReflectionData(result.m_Reflection, &result.m_ErrorMessage);
    if (!result.m_Succeeded)
        result.m_Reflection = {};
    return result;
}
