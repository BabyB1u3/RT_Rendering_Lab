#pragma once

/// @file ShaderParameterWriter.h
/// @brief Reflection-driven helper for writing shader parameters into a ResourceSet.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Util/Math.h"
#include "Render/RHI/RHIResources.h"
#include "Render/Shader/ShaderTypes.h"

struct FieldHandle
{
    static constexpr uint32_t k_InvalidId = std::numeric_limits<uint32_t>::max();

    uint32_t m_Id = k_InvalidId;

    bool IsValid() const { return m_Id != k_InvalidId; }
};

struct BindingHandle
{
    static constexpr uint32_t k_InvalidId = std::numeric_limits<uint32_t>::max();

    uint32_t m_Id = k_InvalidId;

    bool IsValid() const { return m_Id != k_InvalidId; }
};

class ShaderParameterWriter
{
public:
    explicit ShaderParameterWriter(const ShaderReflectionData& reflection);

    FieldHandle ResolveField(std::string_view path) const;
    BindingHandle ResolveBinding(std::string_view path) const;

    void SetFloat(ResourceSet& resourceSet, FieldHandle fieldHandle, float value) const;
    void SetFloat4(ResourceSet& resourceSet, FieldHandle fieldHandle, const Math::Vec4& value) const;
    void SetMatrix4x4(ResourceSet& resourceSet, FieldHandle fieldHandle, const Math::Mat4& value) const;
    void SetTexture(ResourceSet& resourceSet, BindingHandle bindingHandle, Texture* texture) const;
    void SetSampler(ResourceSet& resourceSet, BindingHandle bindingHandle, Sampler* sampler) const;
    void SetBuffer(
        ResourceSet& resourceSet, BindingHandle bindingHandle, Buffer* buffer, uint64_t offset, uint64_t size) const;

    void SetFloat(ResourceSet& resourceSet, std::string_view path, float value) const;
    void SetFloat4(ResourceSet& resourceSet, std::string_view path, const Math::Vec4& value) const;
    void SetMatrix4x4(ResourceSet& resourceSet, std::string_view path, const Math::Mat4& value) const;
    void SetTexture(ResourceSet& resourceSet, std::string_view path, Texture* texture) const;
    void SetSampler(ResourceSet& resourceSet, std::string_view path, Sampler* sampler) const;
    void
    SetBuffer(ResourceSet& resourceSet, std::string_view path, Buffer* buffer, uint64_t offset, uint64_t size) const;

private:
    struct FieldInfo
    {
        uint32_t m_SetIndex = 0;
        uint32_t m_Offset = 0;
        uint32_t m_Size = 0;
    };

    struct BindingInfo
    {
        uint32_t m_SetIndex = 0;
        uint32_t m_Binding = 0;
        ReflectedTypeKind m_TypeKind = ReflectedTypeKind::ConstantData;
        uint32_t m_ArrayCount = 1;
    };

    const FieldInfo& GetFieldInfo(FieldHandle fieldHandle) const;
    const BindingInfo& GetBindingInfo(BindingHandle bindingHandle) const;

    void BuildReflectionLookupTables(const ReflectedField& field,
                                     std::string_view pathPrefix,
                                     uint32_t currentSetIndex,
                                     bool hasSetIndex,
                                     uint32_t constantBaseOffset);
    void AddFieldPath(std::string path, const FieldInfo& fieldInfo);
    void AddBindingPath(std::string path, const BindingInfo& bindingInfo);
    void SetConstantData(ResourceSet& resourceSet, FieldHandle fieldHandle, const void* data, size_t size) const;
    void ValidateResourceSetSetIndex(const ResourceSet& resourceSet,
                                     uint32_t expectedSetIndex,
                                     std::string_view pathKind) const;

private:
    std::vector<FieldInfo> m_Fields;
    std::vector<BindingInfo> m_Bindings;
    std::unordered_map<std::string, uint32_t> m_FieldPathToId;
    std::unordered_map<std::string, uint32_t> m_BindingPathToId;
};
