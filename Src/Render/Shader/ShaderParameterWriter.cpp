#include "Render/Shader/ShaderParameterWriter.h"
#include "Render/Shader/ShaderReflection.h"

#include <string>
#include <utility>

#include "Core/Diagnostics/Assert/Assert.h"

namespace
{
bool IsConstantFieldType(ReflectedTypeKind typeKind)
{
    return typeKind == ReflectedTypeKind::ConstantData;
}

std::string MakeChildPath(std::string_view prefix, std::string_view name)
{
    if (prefix.empty())
        return std::string(name);

    std::string path;
    path.reserve(prefix.size() + 1 + name.size());
    path.append(prefix);
    path.push_back('.');
    path.append(name);
    return path;
}
} // namespace

ShaderParameterWriter::ShaderParameterWriter(const ShaderReflectionData& reflection)
{
    std::string validationError;
    RTRLAB_ASSERT_MSG(ValidateShaderReflectionData(reflection, &validationError), validationError.c_str());

    for (const ReflectedField& field : reflection.m_Globals)
        BuildReflectionLookupTables(field, "", 0, false, 0);
}

FieldHandle ShaderParameterWriter::ResolveField(std::string_view path) const
{
    const auto it = m_FieldPathToId.find(std::string(path));
    if (it == m_FieldPathToId.end())
        return {};

    return FieldHandle{it->second};
}

BindingHandle ShaderParameterWriter::ResolveBinding(std::string_view path) const
{
    const auto it = m_BindingPathToId.find(std::string(path));
    if (it == m_BindingPathToId.end())
        return {};

    return BindingHandle{it->second};
}

void ShaderParameterWriter::SetFloat(ResourceSet& resourceSet, FieldHandle fieldHandle, float value) const
{
    SetConstantData(resourceSet, fieldHandle, &value, sizeof(value));
}

void ShaderParameterWriter::SetFloat4(ResourceSet& resourceSet, FieldHandle fieldHandle, const Math::Vec4& value) const
{
    SetConstantData(resourceSet, fieldHandle, &value, sizeof(value));
}

void ShaderParameterWriter::SetMatrix4x4(ResourceSet& resourceSet,
                                         FieldHandle fieldHandle,
                                         const Math::Mat4& value) const
{
    SetConstantData(resourceSet, fieldHandle, &value, sizeof(value));
}

void ShaderParameterWriter::SetTexture(ResourceSet& resourceSet, BindingHandle bindingHandle, Texture* texture) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Texture ||
                          bindingInfo.m_TypeKind == ReflectedTypeKind::StorageTexture,
                      "BindingHandle must resolve to a texture field for SetTexture.");
    ValidateNonArrayBinding(bindingInfo, "SetTexture");

    TextureBinding textureBinding;
    textureBinding.m_Texture = texture;
    resourceSet.SetTexture(bindingInfo.m_Binding, textureBinding);
}

void ShaderParameterWriter::SetTextureArray(ResourceSet& resourceSet,
                                            BindingHandle bindingHandle,
                                            std::span<Texture* const> textures) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Texture ||
                          bindingInfo.m_TypeKind == ReflectedTypeKind::StorageTexture,
                      "BindingHandle must resolve to a texture field for SetTextureArray.");
    ValidateArrayBindingCount(bindingInfo, textures.size(), "SetTextureArray");

    std::vector<TextureBinding> textureBindings(textures.size());
    for (size_t index = 0; index < textures.size(); ++index)
        textureBindings[index].m_Texture = textures[index];
    resourceSet.SetTextureArray(bindingInfo.m_Binding, textureBindings);
}

void ShaderParameterWriter::SetTextureView(ResourceSet& resourceSet,
                                           BindingHandle bindingHandle,
                                           TextureView* textureView) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Texture ||
                          bindingInfo.m_TypeKind == ReflectedTypeKind::StorageTexture,
                      "BindingHandle must resolve to a texture field for SetTextureView.");
    ValidateNonArrayBinding(bindingInfo, "SetTextureView");

    TextureBinding textureBinding;
    textureBinding.m_Texture = textureView != nullptr ? textureView->GetTexture() : nullptr;
    textureBinding.m_View = textureView;
    resourceSet.SetTexture(bindingInfo.m_Binding, textureBinding);
}

void ShaderParameterWriter::SetTextureViewArray(ResourceSet& resourceSet,
                                                BindingHandle bindingHandle,
                                                std::span<TextureView* const> textureViews) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Texture ||
                          bindingInfo.m_TypeKind == ReflectedTypeKind::StorageTexture,
                      "BindingHandle must resolve to a texture field for SetTextureViewArray.");
    ValidateArrayBindingCount(bindingInfo, textureViews.size(), "SetTextureViewArray");

    std::vector<TextureBinding> textureBindings(textureViews.size());
    for (size_t index = 0; index < textureViews.size(); ++index)
    {
        textureBindings[index].m_View = textureViews[index];
        textureBindings[index].m_Texture = textureViews[index] != nullptr ? textureViews[index]->GetTexture() : nullptr;
    }
    resourceSet.SetTextureArray(bindingInfo.m_Binding, textureBindings);
}

void ShaderParameterWriter::SetSampler(ResourceSet& resourceSet, BindingHandle bindingHandle, Sampler* sampler) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Sampler,
                      "BindingHandle must resolve to a sampler field for SetSampler.");
    ValidateNonArrayBinding(bindingInfo, "SetSampler");

    SamplerBinding samplerBinding;
    samplerBinding.m_Sampler = sampler;
    resourceSet.SetSampler(bindingInfo.m_Binding, samplerBinding);
}

void ShaderParameterWriter::SetSamplerArray(ResourceSet& resourceSet,
                                            BindingHandle bindingHandle,
                                            std::span<Sampler* const> samplers) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Sampler,
                      "BindingHandle must resolve to a sampler field for SetSamplerArray.");
    ValidateArrayBindingCount(bindingInfo, samplers.size(), "SetSamplerArray");

    std::vector<SamplerBinding> samplerBindings(samplers.size());
    for (size_t index = 0; index < samplers.size(); ++index)
        samplerBindings[index].m_Sampler = samplers[index];
    resourceSet.SetSamplerArray(bindingInfo.m_Binding, samplerBindings);
}

void ShaderParameterWriter::SetBuffer(
    ResourceSet& resourceSet, BindingHandle bindingHandle, Buffer* buffer, uint64_t offset, uint64_t size) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Buffer,
                      "BindingHandle must resolve to a buffer field for SetBuffer.");
    ValidateNonArrayBinding(bindingInfo, "SetBuffer");

    BufferBinding bufferBinding;
    bufferBinding.m_Buffer = buffer;
    bufferBinding.m_Offset = offset;
    bufferBinding.m_Size = size;
    resourceSet.SetBuffer(bindingInfo.m_Binding, bufferBinding);
}

void ShaderParameterWriter::SetBufferArray(ResourceSet& resourceSet,
                                           BindingHandle bindingHandle,
                                           std::span<const BufferBinding> bufferBindings) const
{
    const BindingInfo& bindingInfo = GetBindingInfo(bindingHandle);
    ValidateResourceSetSetIndex(resourceSet, bindingInfo.m_SetIndex, "resource binding");
    RTRLAB_ASSERT_MSG(bindingInfo.m_TypeKind == ReflectedTypeKind::Buffer,
                      "BindingHandle must resolve to a buffer field for SetBufferArray.");
    ValidateArrayBindingCount(bindingInfo, bufferBindings.size(), "SetBufferArray");

    resourceSet.SetBufferArray(bindingInfo.m_Binding, bufferBindings);
}

void ShaderParameterWriter::SetFloat(ResourceSet& resourceSet, std::string_view path, float value) const
{
    const FieldHandle fieldHandle = ResolveField(path);
    RTRLAB_ASSERT_MSG(fieldHandle.IsValid(), "SetFloat requires a valid reflected constant-data field path.");
    SetFloat(resourceSet, fieldHandle, value);
}

void ShaderParameterWriter::SetFloat4(ResourceSet& resourceSet, std::string_view path, const Math::Vec4& value) const
{
    const FieldHandle fieldHandle = ResolveField(path);
    RTRLAB_ASSERT_MSG(fieldHandle.IsValid(), "SetFloat4 requires a valid reflected constant-data field path.");
    SetFloat4(resourceSet, fieldHandle, value);
}

void ShaderParameterWriter::SetMatrix4x4(ResourceSet& resourceSet, std::string_view path, const Math::Mat4& value) const
{
    const FieldHandle fieldHandle = ResolveField(path);
    RTRLAB_ASSERT_MSG(fieldHandle.IsValid(), "SetMatrix4x4 requires a valid reflected constant-data field path.");
    SetMatrix4x4(resourceSet, fieldHandle, value);
}

void ShaderParameterWriter::SetTexture(ResourceSet& resourceSet, std::string_view path, Texture* texture) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetTexture requires a valid reflected texture-binding path.");
    SetTexture(resourceSet, bindingHandle, texture);
}

void ShaderParameterWriter::SetTextureArray(ResourceSet& resourceSet,
                                            std::string_view path,
                                            std::span<Texture* const> textures) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetTextureArray requires a valid reflected texture-binding path.");
    SetTextureArray(resourceSet, bindingHandle, textures);
}

void ShaderParameterWriter::SetTextureView(ResourceSet& resourceSet,
                                           std::string_view path,
                                           TextureView* textureView) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetTextureView requires a valid reflected texture-binding path.");
    SetTextureView(resourceSet, bindingHandle, textureView);
}

void ShaderParameterWriter::SetTextureViewArray(ResourceSet& resourceSet,
                                                std::string_view path,
                                                std::span<TextureView* const> textureViews) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetTextureViewArray requires a valid reflected texture-binding path.");
    SetTextureViewArray(resourceSet, bindingHandle, textureViews);
}

void ShaderParameterWriter::SetSampler(ResourceSet& resourceSet, std::string_view path, Sampler* sampler) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetSampler requires a valid reflected sampler-binding path.");
    SetSampler(resourceSet, bindingHandle, sampler);
}

void ShaderParameterWriter::SetSamplerArray(ResourceSet& resourceSet,
                                            std::string_view path,
                                            std::span<Sampler* const> samplers) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetSamplerArray requires a valid reflected sampler-binding path.");
    SetSamplerArray(resourceSet, bindingHandle, samplers);
}

void ShaderParameterWriter::SetBuffer(
    ResourceSet& resourceSet, std::string_view path, Buffer* buffer, uint64_t offset, uint64_t size) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetBuffer requires a valid reflected buffer-binding path.");
    SetBuffer(resourceSet, bindingHandle, buffer, offset, size);
}

void ShaderParameterWriter::SetBufferArray(ResourceSet& resourceSet,
                                           std::string_view path,
                                           std::span<const BufferBinding> bufferBindings) const
{
    const BindingHandle bindingHandle = ResolveBinding(path);
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "SetBufferArray requires a valid reflected buffer-binding path.");
    SetBufferArray(resourceSet, bindingHandle, bufferBindings);
}

const ShaderParameterWriter::FieldInfo& ShaderParameterWriter::GetFieldInfo(FieldHandle fieldHandle) const
{
    RTRLAB_ASSERT_MSG(fieldHandle.IsValid(), "FieldHandle must be valid.");
    RTRLAB_ASSERT_MSG(fieldHandle.m_Id < m_Fields.size(),
                      "FieldHandle is out of range for this ShaderParameterWriter.");
    return m_Fields[fieldHandle.m_Id];
}

const ShaderParameterWriter::BindingInfo& ShaderParameterWriter::GetBindingInfo(BindingHandle bindingHandle) const
{
    RTRLAB_ASSERT_MSG(bindingHandle.IsValid(), "BindingHandle must be valid.");
    RTRLAB_ASSERT_MSG(bindingHandle.m_Id < m_Bindings.size(),
                      "BindingHandle is out of range for this ShaderParameterWriter.");
    return m_Bindings[bindingHandle.m_Id];
}

void ShaderParameterWriter::BuildReflectionLookupTables(const ReflectedField& field,
                                                        std::string_view pathPrefix,
                                                        uint32_t currentSetIndex,
                                                        bool hasSetIndex,
                                                        uint32_t constantBaseOffset)
{
    const std::string path = MakeChildPath(pathPrefix, field.m_Name);

    uint32_t resolvedSetIndex = hasSetIndex ? currentSetIndex : field.m_SetIndex;
    bool childHasSetIndex = hasSetIndex;
    uint32_t childSetIndex = currentSetIndex;
    uint32_t childConstantBaseOffset = constantBaseOffset;

    if (field.m_TypeKind == ReflectedTypeKind::ParameterBlock)
    {
        resolvedSetIndex = field.m_SetIndex;
        childHasSetIndex = true;
        childSetIndex = field.m_SetIndex;
        childConstantBaseOffset = 0;
    }

    if (IsConstantFieldType(field.m_TypeKind))
    {
        FieldInfo fieldInfo;
        fieldInfo.m_SetIndex = resolvedSetIndex;
        fieldInfo.m_Offset = childConstantBaseOffset + field.m_Offset;
        fieldInfo.m_Size = field.m_Size;
        AddFieldPath(path, fieldInfo);
    }
    else if (IsBindableReflectedType(field.m_TypeKind))
    {
        BindingInfo bindingInfo;
        bindingInfo.m_SetIndex = resolvedSetIndex;
        bindingInfo.m_Binding = field.m_Binding;
        bindingInfo.m_TypeKind = field.m_TypeKind;
        bindingInfo.m_ArrayCount = field.m_ArrayCount;
        AddBindingPath(path, bindingInfo);
    }

    if (field.m_TypeKind == ReflectedTypeKind::Struct || field.m_TypeKind == ReflectedTypeKind::ConstantData)
        childConstantBaseOffset += field.m_Offset;

    if (field.m_TypeKind == ReflectedTypeKind::ParameterBlock)
        childSetIndex = field.m_SetIndex;

    for (const ReflectedField& child : field.m_Children)
        BuildReflectionLookupTables(child, path, childSetIndex, childHasSetIndex, childConstantBaseOffset);
}

void ShaderParameterWriter::AddFieldPath(std::string path, const FieldInfo& fieldInfo)
{
    const uint32_t nextId = static_cast<uint32_t>(m_Fields.size());
    const auto [it, inserted] = m_FieldPathToId.emplace(std::move(path), nextId);
    RTRLAB_ASSERT_MSG(inserted, "Duplicate reflected constant-data field path.");
    m_Fields.push_back(fieldInfo);
}

void ShaderParameterWriter::AddBindingPath(std::string path, const BindingInfo& bindingInfo)
{
    const uint32_t nextId = static_cast<uint32_t>(m_Bindings.size());
    const auto [it, inserted] = m_BindingPathToId.emplace(std::move(path), nextId);
    RTRLAB_ASSERT_MSG(inserted, "Duplicate reflected resource-binding path.");
    m_Bindings.push_back(bindingInfo);
}

void ShaderParameterWriter::SetConstantData(ResourceSet& resourceSet,
                                            FieldHandle fieldHandle,
                                            const void* data,
                                            size_t size) const
{
    RTRLAB_ASSERT_MSG(data != nullptr, "SetConstantData requires non-null source data.");

    const FieldInfo& fieldInfo = GetFieldInfo(fieldHandle);
    ValidateResourceSetSetIndex(resourceSet, fieldInfo.m_SetIndex, "constant-data field");
    RTRLAB_ASSERT_MSG(size <= fieldInfo.m_Size, "SetConstantData payload exceeds the reflected field size.");

    resourceSet.SetConstantDataRaw(fieldInfo.m_Offset, data, size);
}

void ShaderParameterWriter::ValidateNonArrayBinding(const BindingInfo& bindingInfo,
                                                    std::string_view operationName) const
{
    RTRLAB_ASSERTF(bindingInfo.m_ArrayCount == 1,
                   "{} requires a non-array binding, but the reflected binding has {} elements.",
                   operationName,
                   bindingInfo.m_ArrayCount);
}

void ShaderParameterWriter::ValidateArrayBindingCount(const BindingInfo& bindingInfo,
                                                      size_t providedCount,
                                                      std::string_view operationName) const
{
    RTRLAB_ASSERTF(providedCount == bindingInfo.m_ArrayCount,
                   "{} requires exactly {} element(s), but received {}.",
                   operationName,
                   bindingInfo.m_ArrayCount,
                   providedCount);
}

void ShaderParameterWriter::ValidateResourceSetSetIndex(const ResourceSet& resourceSet,
                                                        uint32_t expectedSetIndex,
                                                        std::string_view pathKind) const
{
    RTRLAB_ASSERTF(resourceSet.GetSetIndex() == expectedSetIndex,
                   "ShaderParameterWriter {} targets set {} but ResourceSet is set {}.",
                   pathKind,
                   expectedSetIndex,
                   resourceSet.GetSetIndex());
}
