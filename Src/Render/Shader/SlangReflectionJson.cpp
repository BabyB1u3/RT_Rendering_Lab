#include "Render/Shader/SlangReflectionJson.h"

#include <memory>
#include <utility>

#include <json.hpp>

namespace
{
using Json = nlohmann::json;

void AssignSlangReflectionJsonError(std::string* errorMessage, std::string message)
{
    if (errorMessage != nullptr)
        *errorMessage = std::move(message);
}

std::optional<uint32_t> ReadUint32(const Json& json, const char* key)
{
    if (!json.contains(key) || !json[key].is_number_unsigned())
        return std::nullopt;

    return json[key].get<uint32_t>();
}

std::optional<std::string> ReadString(const Json& json, const char* key)
{
    if (!json.contains(key) || !json[key].is_string())
        return std::nullopt;

    return json[key].get<std::string>();
}

std::optional<SlangReflectionBinding> ParseBinding(const Json& json, std::string* errorMessage)
{
    if (json.is_null())
        return std::nullopt;

    if (!json.is_object())
    {
        AssignSlangReflectionJsonError(errorMessage, "Slang reflection binding payload must be a JSON object.");
        return std::nullopt;
    }

    SlangReflectionBinding binding;
    binding.m_Kind = ReadString(json, "kind").value_or(std::string{});
    binding.m_Index = ReadUint32(json, "index").value_or(0);
    binding.m_Space = ReadUint32(json, "space").value_or(0);
    binding.m_HasExplicitSpace = json.contains("space") && json["space"].is_number_unsigned();
    binding.m_Offset = ReadUint32(json, "offset").value_or(0);
    binding.m_Size = ReadUint32(json, "size").value_or(0);
    return binding;
}

std::optional<SlangReflectionBinding> SelectPreferredBinding(const Json& json, std::string* errorMessage)
{
    if (json.contains("binding"))
    {
        std::optional<SlangReflectionBinding> binding = ParseBinding(json["binding"], errorMessage);
        if (!binding.has_value() && !json["binding"].is_null())
            return std::nullopt;
        if (binding.has_value())
            return binding;
    }

    if (!json.contains("bindings"))
        return std::nullopt;

    if (!json["bindings"].is_array())
    {
        AssignSlangReflectionJsonError(errorMessage, "Slang reflection bindings payload must be an array.");
        return std::nullopt;
    }

    std::optional<SlangReflectionBinding> fallbackBinding;
    std::optional<SlangReflectionBinding> preferredBinding;
    std::optional<uint32_t> inferredRegisterSpace;
    for (const Json& bindingJson : json["bindings"])
    {
        std::optional<SlangReflectionBinding> binding = ParseBinding(bindingJson, errorMessage);
        if (!binding.has_value())
            return std::nullopt;

        if (binding->m_Kind == "subElementRegisterSpace")
        {
            inferredRegisterSpace = binding->m_Index;
            if (!fallbackBinding.has_value())
                fallbackBinding = *binding;
            continue;
        }

        if (binding->m_Kind == "descriptorTableSlot" || binding->m_Kind == "constantBuffer" ||
            binding->m_Kind == "uniform")
        {
            if (!preferredBinding.has_value())
                preferredBinding = *binding;
            continue;
        }

        if (!fallbackBinding.has_value())
            fallbackBinding = *binding;
    }

    if (preferredBinding.has_value())
    {
        if (preferredBinding->m_Kind == "descriptorTableSlot" && !preferredBinding->m_HasExplicitSpace &&
            inferredRegisterSpace.has_value())
        {
            preferredBinding->m_Space = *inferredRegisterSpace;
        }
        return preferredBinding;
    }

    return fallbackBinding;
}

bool ParseType(const Json& json, SlangReflectionType& outType, std::string* errorMessage);

bool ParseField(const Json& json, SlangReflectionField& outField, std::string* errorMessage)
{
    if (!json.is_object())
    {
        AssignSlangReflectionJsonError(errorMessage, "Slang reflection field payload must be a JSON object.");
        return false;
    }

    outField.m_Name = ReadString(json, "name").value_or(std::string{});
    if (json.contains("type"))
    {
        outField.m_Type = std::make_unique<SlangReflectionType>();
        if (!ParseType(json["type"], *outField.m_Type, errorMessage))
            return false;
    }

    if (json.contains("binding"))
    {
        outField.m_Binding = ParseBinding(json["binding"], errorMessage);
        if (!outField.m_Binding.has_value() && !json["binding"].is_null())
            return false;
    }

    return true;
}

bool ParseType(const Json& json, SlangReflectionType& outType, std::string* errorMessage)
{
    if (!json.is_object())
    {
        AssignSlangReflectionJsonError(errorMessage, "Slang reflection type payload must be a JSON object.");
        return false;
    }

    outType.m_Kind = ReadString(json, "kind").value_or(std::string{});
    outType.m_Name = ReadString(json, "name").value_or(std::string{});
    outType.m_ScalarType = ReadString(json, "scalarType").value_or(std::string{});
    outType.m_ElementCount = ReadUint32(json, "elementCount").value_or(0);
    outType.m_RowCount = ReadUint32(json, "rowCount").value_or(0);
    outType.m_ColumnCount = ReadUint32(json, "columnCount").value_or(0);

    if (json.contains("elementType"))
    {
        outType.m_ElementType = std::make_unique<SlangReflectionType>();
        if (!ParseType(json["elementType"], *outType.m_ElementType, errorMessage))
            return false;
    }

    if (json.contains("elementVarLayout"))
    {
        const Json& elementVarLayoutJson = json["elementVarLayout"];
        if (!elementVarLayoutJson.is_object())
        {
            AssignSlangReflectionJsonError(errorMessage, "Slang reflection type.elementVarLayout must be an object.");
            return false;
        }

        if (elementVarLayoutJson.contains("binding"))
        {
            outType.m_ElementVarBinding = ParseBinding(elementVarLayoutJson["binding"], errorMessage);
            if (!outType.m_ElementVarBinding.has_value() && !elementVarLayoutJson["binding"].is_null())
                return false;
        }
    }

    if (json.contains("fields"))
    {
        if (!json["fields"].is_array())
        {
            AssignSlangReflectionJsonError(errorMessage, "Slang reflection type.fields must be an array.");
            return false;
        }

        outType.m_Fields.reserve(json["fields"].size());
        for (const Json& fieldJson : json["fields"])
        {
            SlangReflectionField field;
            if (!ParseField(fieldJson, field, errorMessage))
                return false;
            outType.m_Fields.push_back(std::move(field));
        }
    }

    if (json.contains("genericArgs"))
    {
        if (!json["genericArgs"].is_array())
        {
            AssignSlangReflectionJsonError(errorMessage, "Slang reflection type.genericArgs must be an array.");
            return false;
        }

        outType.m_GenericArguments.reserve(json["genericArgs"].size());
        for (const Json& argumentJson : json["genericArgs"])
        {
            SlangReflectionType argumentType;
            if (!ParseType(argumentJson, argumentType, errorMessage))
                return false;
            outType.m_GenericArguments.push_back(std::move(argumentType));
        }
    }

    return true;
}

bool ParseParameter(const Json& json, SlangReflectionParameter& outParameter, std::string* errorMessage)
{
    if (!json.is_object())
    {
        AssignSlangReflectionJsonError(errorMessage, "Slang reflection parameter payload must be a JSON object.");
        return false;
    }

    outParameter.m_Name = ReadString(json, "name").value_or(std::string{});
    if (json.contains("type"))
    {
        if (!ParseType(json["type"], outParameter.m_Type, errorMessage))
            return false;
    }

    outParameter.m_Binding = SelectPreferredBinding(json, errorMessage);
    if (json.contains("binding") || json.contains("bindings"))
    {
        if (!outParameter.m_Binding.has_value() && errorMessage != nullptr && !errorMessage->empty())
            return false;
    }

    return true;
}

bool ParseEntryPointBinding(const Json& json, SlangReflectionEntryPointBinding& outBinding, std::string* errorMessage)
{
    if (!json.is_object())
    {
        AssignSlangReflectionJsonError(errorMessage,
                                       "Slang reflection entry-point binding payload must be a JSON object.");
        return false;
    }

    outBinding.m_Name = ReadString(json, "name").value_or(std::string{});

    std::optional<SlangReflectionBinding> binding = SelectPreferredBinding(json, errorMessage);
    if ((json.contains("binding") || json.contains("bindings")) && !binding.has_value() && errorMessage != nullptr &&
        !errorMessage->empty())
    {
        return false;
    }

    if (!binding.has_value())
    {
        AssignSlangReflectionJsonError(errorMessage,
                                       "Slang reflection entry-point bindings require a binding payload.");
        return false;
    }

    outBinding.m_Binding = std::move(*binding);
    return true;
}

bool ParseEntryPoint(const Json& json, SlangReflectionEntryPoint& outEntryPoint, std::string* errorMessage)
{
    if (!json.is_object())
    {
        AssignSlangReflectionJsonError(errorMessage, "Slang reflection entry-point payload must be a JSON object.");
        return false;
    }

    outEntryPoint.m_Name = ReadString(json, "name").value_or(std::string{});
    outEntryPoint.m_Stage = ReadString(json, "stage").value_or(std::string{});

    if (json.contains("bindings"))
    {
        if (!json["bindings"].is_array())
        {
            AssignSlangReflectionJsonError(errorMessage, "Slang reflection entryPoint.bindings must be an array.");
            return false;
        }

        outEntryPoint.m_Bindings.reserve(json["bindings"].size());
        for (const Json& bindingJson : json["bindings"])
        {
            SlangReflectionEntryPointBinding binding;
            if (!ParseEntryPointBinding(bindingJson, binding, errorMessage))
                return false;
            outEntryPoint.m_Bindings.push_back(std::move(binding));
        }
    }

    return true;
}

bool ValidateDocument(const SlangReflectionDocument& document, std::string* errorMessage)
{
    for (const SlangReflectionParameter& parameter : document.m_Parameters)
    {
        if (parameter.m_Name.empty())
        {
            AssignSlangReflectionJsonError(errorMessage, "Slang reflection parameters require a non-empty name.");
            return false;
        }
    }

    for (const SlangReflectionEntryPoint& entryPoint : document.m_EntryPoints)
    {
        if (entryPoint.m_Stage.empty())
        {
            AssignSlangReflectionJsonError(errorMessage, "Slang reflection entry points require a non-empty stage.");
            return false;
        }
    }

    return true;
}
} // namespace

ParseSlangReflectionResult ParseSlangReflectionJson(std::string_view jsonText)
{
    ParseSlangReflectionResult result;

    try
    {
        const Json json = Json::parse(std::string(jsonText));

        if (json.contains("parameters"))
        {
            if (!json["parameters"].is_array())
            {
                result.m_ErrorMessage = "Slang reflection document.parameters must be an array.";
                return result;
            }

            result.m_Document.m_Parameters.reserve(json["parameters"].size());
            for (const Json& parameterJson : json["parameters"])
            {
                SlangReflectionParameter parameter;
                if (!ParseParameter(parameterJson, parameter, &result.m_ErrorMessage))
                    return result;
                result.m_Document.m_Parameters.push_back(std::move(parameter));
            }
        }

        if (json.contains("entryPoints"))
        {
            if (!json["entryPoints"].is_array())
            {
                result.m_ErrorMessage = "Slang reflection document.entryPoints must be an array.";
                return result;
            }

            result.m_Document.m_EntryPoints.reserve(json["entryPoints"].size());
            for (const Json& entryPointJson : json["entryPoints"])
            {
                SlangReflectionEntryPoint entryPoint;
                if (!ParseEntryPoint(entryPointJson, entryPoint, &result.m_ErrorMessage))
                    return result;
                result.m_Document.m_EntryPoints.push_back(std::move(entryPoint));
            }
        }

        result.m_Succeeded = ValidateDocument(result.m_Document, &result.m_ErrorMessage);
        if (!result.m_Succeeded)
            result.m_Document = {};
    }
    catch (const std::exception& exception)
    {
        result.m_ErrorMessage = exception.what();
    }

    return result;
}
