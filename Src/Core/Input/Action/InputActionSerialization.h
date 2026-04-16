#pragma once

/// @file InputActionSerialization.h
/// @brief Serialize/Deserialize traits for InputActionMap.
///
/// Produces the same JSON structure as the old inline SaveToFile/LoadFromFile:
///
///   {
///     "actions": {
///       "Name": [
///         { "type": "Key", "code": "W" },
///         { "kind": "Chord", "sources": [ ... ] }
///       ]
///     },
///     "axes": { "Name": { "kind": "KeyPair", "positive": "W", "negative": "S" } }
///   }
///
/// InputSource::Type and MouseAxis use magic_enum for token conversion.
/// Key/Mouse/Gamepad codes continue to use InputNames.h for stable canonical names.

#include "Core/Serialization/BuiltinTraits.h"
#include "Core/Diagnostics/Logging/LogCategories.h"
#include "Core/Diagnostics/Logging/LogMacros.h"
#include "Core/Input/Action/InputAction.h"
#include "Core/Input/Code/InputNames.h"

namespace Serialization
{

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
// InputSource
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

inline void Serialize(PropertyTree& tree, const InputSource& src)
{
    tree = PropertyTree::Object{};
    // InputSource::Type uses magic_enum (tokens: "Key", "MouseButton", etc.)
    PropertyTree typeTree;
    Serialize(typeTree, src.sourceType);
    tree["type"] = std::move(typeTree);

    // Code uses InputNames.h for stable names
    if (src.sourceType == InputSource::Type::MouseButton)
        tree["code"] = PropertyTree(Mouse::ToName(static_cast<Mouse::Code>(src.code)));
    else if (src.sourceType == InputSource::Type::GamepadButton)
        tree["code"] = PropertyTree(GamepadButton::ToName(static_cast<GamepadButton::Code>(src.code)));
    else if (src.sourceType == InputSource::Type::GamepadAxis)
        tree["code"] = PropertyTree(GamepadAxis::ToName(static_cast<GamepadAxis::Code>(src.code)));
    else
        tree["code"] = PropertyTree(Key::ToName(static_cast<Key::Code>(src.code)));

    if (src.deviceIndex != 0)
        tree["device"] = PropertyTree(static_cast<int>(src.deviceIndex));
}

inline bool Deserialize(const PropertyTree& tree, InputSource& src)
{
    if (!tree.IsObject())
        return false;

    // Parse type (default to Key)
    InputSource::Type type = InputSource::Type::Key;
    if (tree.HasKey("type"))
        Deserialize(tree["type"], type);

    // Parse code name
    std::string codeName;
    if (tree.HasKey("code"))
        Deserialize(tree["code"], codeName);

    uint16_t code;
    if (type == InputSource::Type::MouseButton)
    {
        code = Mouse::FromName(codeName);
        if (code == Mouse::InvalidCode)
            return false;
    }
    else if (type == InputSource::Type::GamepadButton)
    {
        code = GamepadButton::FromName(codeName);
        if (code == GamepadButton::InvalidCode)
            return false;
    }
    else if (type == InputSource::Type::GamepadAxis)
    {
        code = GamepadAxis::FromName(codeName);
        if (code == GamepadAxis::InvalidCode)
            return false;
    }
    else
    {
        code = Key::FromName(codeName);
        if (type == InputSource::Type::Key && code == Key::InvalidCode)
            return false;
    }

    src.sourceType = type;
    src.code = code;
    src.deviceIndex = static_cast<uint8_t>(tree.GetOr<int>("device", 0));
    return true;
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?
// ChordBinding
inline void Serialize(PropertyTree& tree, const ChordBinding& binding)
{
    tree = PropertyTree::Object{};
    tree["kind"] = PropertyTree("Chord");

    PropertyTree sourcesTree;
    Serialize(sourcesTree, binding.sources);
    tree["sources"] = std::move(sourcesTree);
}

inline bool Deserialize(const PropertyTree& tree, ChordBinding& binding)
{
    if (!tree.IsObject())
        return false;

    if (tree.HasKey("kind"))
    {
        std::string kind;
        Deserialize(tree["kind"], kind);
        if (kind != "Chord")
            return false;
    }

    if (!tree.HasKey("sources"))
        return false;

    std::vector<InputSource> sources;
    if (!Deserialize(tree["sources"], sources) || sources.empty())
        return false;

    binding.sources = std::move(sources);
    return true;
}

// InputActionMap
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲锟?

inline void Serialize(PropertyTree& tree, const InputActionMap& map)
{
    tree = PropertyTree::Object{};

    // Serialize actions
    PropertyTree actionsTree = PropertyTree::Object{};
    for (const auto& [name, sources] : map.GetActions())
    {
        PropertyTree arr;
        Serialize(arr, sources); // std::vector<InputSource>
        actionsTree[name] = std::move(arr);
    }

    for (const auto& [name, chords] : map.GetChordActions())
    {
        if (!actionsTree.HasKey(name))
            actionsTree[name] = PropertyTree::Array{};

        auto& arr = actionsTree[name].AsArray();
        for (const auto& chord : chords)
        {
            PropertyTree chordTree;
            Serialize(chordTree, chord);
            arr.push_back(std::move(chordTree));
        }
    }
    tree["actions"] = std::move(actionsTree);

    // Serialize axes
    PropertyTree axesTree = PropertyTree::Object{};
    for (const auto& [name, entry] : map.GetAxes())
    {
        PropertyTree axisObj = PropertyTree::Object{};
        if (entry.kind == InputActionMap::AxisEntry::Kind::KeyPair)
        {
            axisObj["kind"] = PropertyTree("KeyPair");
            axisObj["positive"] = PropertyTree(Key::ToName(static_cast<Key::Code>(entry.keyPair.positive.code)));
            axisObj["negative"] = PropertyTree(Key::ToName(static_cast<Key::Code>(entry.keyPair.negative.code)));
        }
        else if (entry.kind == InputActionMap::AxisEntry::Kind::MouseAxis)
        {
            axisObj["kind"] = PropertyTree("MouseAxis");
            PropertyTree mouseAxisTree;
            Serialize(mouseAxisTree, entry.mouseAxis);
            axisObj["mouseAxis"] = std::move(mouseAxisTree);
        }
        else
        {
            axisObj["kind"] = PropertyTree("GamepadAxis");
            axisObj["gamepadAxis"] = PropertyTree(GamepadAxis::ToName(entry.gamepadAxis));
            if (entry.deviceIndex != 0)
                axisObj["device"] = PropertyTree(static_cast<int>(entry.deviceIndex));
        }
        axesTree[name] = std::move(axisObj);
    }
    tree["axes"] = std::move(axesTree);
}

inline bool Deserialize(const PropertyTree& tree, InputActionMap& map)
{
    if (!tree.IsObject())
        return false;

    // Parse into temporaries - only commit on full success.
    InputActionMap temp;

    // Parse actions
    if (tree.HasKey("actions") && tree["actions"].IsObject())
    {
        for (const auto& [name, arr] : tree["actions"].AsObject())
        {
            if (!arr.IsArray())
                continue;

            for (const auto& srcTree : arr.AsArray())
            {
                ChordBinding chord{};
                if (Deserialize(srcTree, chord))
                {
                    temp.BindChordAction(name, chord.sources);
                    continue;
                }

                InputSource src{};
                if (!Deserialize(srcTree, src))
                {
                    // Log warning with code name for diagnostics
                    std::string codeName;
                    if (srcTree.IsObject() && srcTree.HasKey("code"))
                        codeName = srcTree["code"].AsString();
                    std::string typeName;
                    if (srcTree.IsObject() && srcTree.HasKey("type"))
                        typeName = srcTree["type"].AsString();
                    std::string kindName;
                    if (srcTree.IsObject() && srcTree.HasKey("kind"))
                        kindName = srcTree["kind"].AsString();

                    if (!codeName.empty())
                    {
                        LOG_WARN_CAT(LogCategory::k_Input,
                                     "InputActionMap: unknown {} '{}' in action '{}'",
                                     typeName.empty() ? "key" : typeName,
                                     codeName,
                                     name);
                    }
                    else if (!kindName.empty())
                    {
                        LOG_WARN_CAT(LogCategory::k_Input,
                                     "InputActionMap: unsupported action binding kind '{}' in '{}'",
                                     kindName,
                                     name);
                    }
                    continue;
                }
                temp.BindAction(name, src);
            }
        }
    }

    // Parse axes
    if (tree.HasKey("axes") && tree["axes"].IsObject())
    {
        for (const auto& [name, axisObj] : tree["axes"].AsObject())
        {
            if (!axisObj.IsObject())
                continue;

            std::string kindStr;
            if (axisObj.HasKey("kind"))
                Deserialize(axisObj["kind"], kindStr);

            if (kindStr == "KeyPair")
            {
                std::string posName, negName;
                if (axisObj.HasKey("positive"))
                    Deserialize(axisObj["positive"], posName);
                if (axisObj.HasKey("negative"))
                    Deserialize(axisObj["negative"], negName);

                auto posCode = Key::FromName(posName);
                auto negCode = Key::FromName(negName);

                if (posCode == Key::InvalidCode || negCode == Key::InvalidCode)
                {
                    LOG_WARN_CAT(LogCategory::k_Input,
                                 "InputActionMap: invalid KeyPair axis '{}' (positive='{}', negative='{}')",
                                 name,
                                 posName,
                                 negName);
                    continue;
                }
                temp.BindAxis(name, posCode, negCode);
            }
            else if (kindStr == "MouseAxis")
            {
                InputActionMap::MouseAxis mouseAxis = InputActionMap::MouseAxis::X;
                if (axisObj.HasKey("mouseAxis"))
                    Deserialize(axisObj["mouseAxis"], mouseAxis);
                temp.BindAxis(name, mouseAxis);
            }
            else if (kindStr == "GamepadAxis")
            {
                std::string axisName;
                if (axisObj.HasKey("gamepadAxis"))
                    Deserialize(axisObj["gamepadAxis"], axisName);

                const auto axisCode = GamepadAxis::FromName(axisName);
                if (axisCode == GamepadAxis::InvalidCode)
                {
                    LOG_WARN_CAT(
                        LogCategory::k_Input, "InputActionMap: invalid GamepadAxis '{}' for '{}'", axisName, name);
                    continue;
                }

                temp.BindAxis(name, axisCode, static_cast<uint8_t>(axisObj.GetOr<int>("device", 0)));
            }
            else
            {
                LOG_WARN_CAT(LogCategory::k_Input, "InputActionMap: unknown axis kind '{}' for '{}'", kindStr, name);
            }
        }
    }

    map = std::move(temp);
    return true;
}

} // namespace Serialization
