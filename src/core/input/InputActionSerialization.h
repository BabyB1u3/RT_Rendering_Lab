#pragma once

/// @file InputActionSerialization.h
/// @brief Serialize/Deserialize traits for InputActionMap.
///
/// Produces the same JSON structure as the old inline SaveToFile/LoadFromFile:
///
///   {
///     "actions": { "Name": [ { "type": "Key", "code": "W" }, ... ] },
///     "axes":    { "Name": { "kind": "KeyPair", "positive": "W", "negative": "S" } }
///   }
///
/// InputSource::Type and MouseAxis use magic_enum for token conversion.
/// Key::Code and Mouse::Code continue to use InputNames.h for stable canonical names.

#include "core/serialization/BuiltinTraits.h"
#include "core/input/InputAction.h"
#include "core/input/InputNames.h"

namespace Serialization
{

    // ═════════════════════════════════════════════════════════════════════
    // InputSource
    // ═════════════════════════════════════════════════════════════════════

    inline void Serialize(PropertyTree &tree, const InputSource &src)
    {
        tree = PropertyTree::Object{};
        // InputSource::Type uses magic_enum (tokens: "Key", "MouseButton", etc.)
        PropertyTree typeTree;
        Serialize(typeTree, src.SourceType);
        tree["type"] = std::move(typeTree);

        // Code uses InputNames.h for stable names
        if (src.SourceType == InputSource::Type::MouseButton)
            tree["code"] = PropertyTree(Mouse::ToName(static_cast<Mouse::Code>(src.Code)));
        else
            tree["code"] = PropertyTree(Key::ToName(static_cast<Key::Code>(src.Code)));

        if (src.DeviceIndex != 0)
            tree["device"] = PropertyTree(static_cast<int>(src.DeviceIndex));
    }

    inline bool Deserialize(const PropertyTree &tree, InputSource &src)
    {
        if (!tree.IsObject())
            return false;

        // Parse type (default to Key)
        InputSource::Type type = InputSource::Type::Key;
        if (tree.Contains("type"))
            Deserialize(tree["type"], type);

        // Parse code name
        std::string codeName;
        if (tree.Contains("code"))
            Deserialize(tree["code"], codeName);

        uint16_t code;
        if (type == InputSource::Type::MouseButton)
        {
            code = Mouse::FromName(codeName);
            if (code == Mouse::InvalidCode)
                return false;
        }
        else
        {
            code = Key::FromName(codeName);
            if (type == InputSource::Type::Key && code == Key::InvalidCode)
                return false;
        }

        src.SourceType = type;
        src.Code = code;
        src.DeviceIndex = static_cast<uint8_t>(tree.GetOr<int>("device", 0));
        return true;
    }

    // ═════════════════════════════════════════════════════════════════════
    // InputActionMap
    // ═════════════════════════════════════════════════════════════════════

    inline void Serialize(PropertyTree &tree, const InputActionMap &map)
    {
        tree = PropertyTree::Object{};

        // Serialize actions
        PropertyTree actionsTree = PropertyTree::Object{};
        for (const auto &[name, sources] : map.GetActions())
        {
            PropertyTree arr;
            Serialize(arr, sources); // std::vector<InputSource>
            actionsTree[name] = std::move(arr);
        }
        tree["actions"] = std::move(actionsTree);

        // Serialize axes
        PropertyTree axesTree = PropertyTree::Object{};
        for (const auto &[name, entry] : map.GetAxes())
        {
            PropertyTree axisObj = PropertyTree::Object{};
            if (entry.kind == InputActionMap::AxisEntry::Kind::KeyPair)
            {
                axisObj["kind"] = PropertyTree("KeyPair");
                axisObj["positive"] = PropertyTree(
                    Key::ToName(static_cast<Key::Code>(entry.keyPair.Positive.Code)));
                axisObj["negative"] = PropertyTree(
                    Key::ToName(static_cast<Key::Code>(entry.keyPair.Negative.Code)));
            }
            else
            {
                axisObj["kind"] = PropertyTree("MouseAxis");
                PropertyTree mouseAxisTree;
                Serialize(mouseAxisTree, entry.mouseAxis);
                axisObj["mouseAxis"] = std::move(mouseAxisTree);
            }
            axesTree[name] = std::move(axisObj);
        }
        tree["axes"] = std::move(axesTree);
    }

    inline bool Deserialize(const PropertyTree &tree, InputActionMap &map)
    {
        if (!tree.IsObject())
            return false;

        // Parse into temporaries — only commit on full success.
        InputActionMap temp;

        // Parse actions
        if (tree.Contains("actions") && tree["actions"].IsObject())
        {
            for (const auto &[name, arr] : tree["actions"].AsObject())
            {
                if (!arr.IsArray())
                    continue;

                for (const auto &srcTree : arr.AsArray())
                {
                    InputSource src{};
                    if (!Deserialize(srcTree, src))
                    {
                        // Log warning with code name for diagnostics
                        std::string codeName;
                        if (srcTree.IsObject() && srcTree.Contains("code"))
                            codeName = srcTree["code"].AsString();
                        std::string typeName;
                        if (srcTree.IsObject() && srcTree.Contains("type"))
                            typeName = srcTree["type"].AsString();

                        if (!codeName.empty())
                        {
                            LOG_WARN("InputActionMap: unknown {} '{}' in action '{}'",
                                     typeName.empty() ? "key" : typeName, codeName, name);
                        }
                        continue;
                    }
                    temp.BindAction(name, src);
                }
            }
        }

        // Parse axes
        if (tree.Contains("axes") && tree["axes"].IsObject())
        {
            for (const auto &[name, axisObj] : tree["axes"].AsObject())
            {
                if (!axisObj.IsObject())
                    continue;

                std::string kindStr;
                if (axisObj.Contains("kind"))
                    Deserialize(axisObj["kind"], kindStr);

                if (kindStr == "KeyPair")
                {
                    std::string posName, negName;
                    if (axisObj.Contains("positive"))
                        Deserialize(axisObj["positive"], posName);
                    if (axisObj.Contains("negative"))
                        Deserialize(axisObj["negative"], negName);

                    auto posCode = Key::FromName(posName);
                    auto negCode = Key::FromName(negName);

                    if (posCode == Key::InvalidCode || negCode == Key::InvalidCode)
                    {
                        LOG_WARN("InputActionMap: invalid KeyPair axis '{}' (positive='{}', negative='{}')",
                                 name, posName, negName);
                        continue;
                    }
                    temp.BindAxis(name, posCode, negCode);
                }
                else if (kindStr == "MouseAxis")
                {
                    InputActionMap::MouseAxis mouseAxis = InputActionMap::MouseAxis::X;
                    if (axisObj.Contains("mouseAxis"))
                        Deserialize(axisObj["mouseAxis"], mouseAxis);
                    temp.BindAxis(name, mouseAxis);
                }
                else
                {
                    LOG_WARN("InputActionMap: unknown axis kind '{}' for '{}'", kindStr, name);
                }
            }
        }

        map = std::move(temp);
        return true;
    }

} // namespace Serialization
