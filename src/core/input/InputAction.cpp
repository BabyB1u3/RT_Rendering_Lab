#include "core/input/InputAction.h"
#include "core/input/Input.h"
#include "core/input/InputNames.h"
#include "core/Logger.h"

#include <json.hpp>
#include <fstream>

// --- Registration ---

void InputActionMap::BindAction(const std::string &name, InputSource source)
{
    m_Actions[name].push_back(source);
}

void InputActionMap::BindAction(const std::string &name, Key::Code key)
{
    BindAction(name, InputSource::FromKey(key));
}

void InputActionMap::BindAxis(const std::string &name, Key::Code positive, Key::Code negative)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::KeyPair;
    entry.keyPair.Positive = InputSource::FromKey(positive);
    entry.keyPair.Negative = InputSource::FromKey(negative);
    m_Axes[name] = entry;
}

void InputActionMap::BindAxis(const std::string &name, MouseAxis mouseAxis)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::MouseAxis;
    entry.mouseAxis = mouseAxis;
    m_Axes[name] = entry;
}

void InputActionMap::Unbind(const std::string &name)
{
    m_Actions.erase(name);
    m_Axes.erase(name);
}

void InputActionMap::Clear()
{
    m_Actions.clear();
    m_Axes.clear();
}

// --- Serialization ---

namespace
{
    const char *SourceTypeName(InputSource::Type type)
    {
        switch (type)
        {
        case InputSource::Type::Key:           return "Key";
        case InputSource::Type::MouseButton:   return "MouseButton";
        case InputSource::Type::GamepadButton: return "GamepadButton";
        case InputSource::Type::GamepadAxis:   return "GamepadAxis";
        }
        return "Key";
    }

    InputSource::Type ParseSourceType(const std::string &str)
    {
        if (str == "MouseButton")   return InputSource::Type::MouseButton;
        if (str == "GamepadButton") return InputSource::Type::GamepadButton;
        if (str == "GamepadAxis")   return InputSource::Type::GamepadAxis;
        return InputSource::Type::Key;
    }

    const char *MouseAxisName(InputActionMap::MouseAxis axis)
    {
        switch (axis)
        {
        case InputActionMap::MouseAxis::X:       return "X";
        case InputActionMap::MouseAxis::Y:       return "Y";
        case InputActionMap::MouseAxis::ScrollY: return "ScrollY";
        }
        return "X";
    }

    InputActionMap::MouseAxis ParseMouseAxis(const std::string &str)
    {
        if (str == "Y")       return InputActionMap::MouseAxis::Y;
        if (str == "ScrollY") return InputActionMap::MouseAxis::ScrollY;
        return InputActionMap::MouseAxis::X;
    }

    // Resolve a code name to its numeric value depending on source type.
    std::string SourceCodeToName(InputSource::Type type, uint16_t code)
    {
        if (type == InputSource::Type::MouseButton)
            return Mouse::ToName(static_cast<Mouse::Code>(code));
        return Key::ToName(static_cast<Key::Code>(code));
    }

    uint16_t SourceCodeFromName(InputSource::Type type, const std::string &name)
    {
        if (type == InputSource::Type::MouseButton)
            return Mouse::FromName(name);
        return Key::FromName(name);
    }
} // namespace

bool InputActionMap::SaveToFile(const std::string &path) const
{
    using json = nlohmann::json;

    json root;

    // Serialize actions
    json actionsObj = json::object();
    for (const auto &[name, sources] : m_Actions)
    {
        json arr = json::array();
        for (const auto &src : sources)
        {
            json srcObj;
            srcObj["type"] = SourceTypeName(src.SourceType);
            srcObj["code"] = SourceCodeToName(src.SourceType, src.Code);
            if (src.DeviceIndex != 0)
                srcObj["device"] = src.DeviceIndex;
            arr.push_back(std::move(srcObj));
        }
        actionsObj[name] = std::move(arr);
    }
    root["actions"] = std::move(actionsObj);

    // Serialize axes
    json axesObj = json::object();
    for (const auto &[name, entry] : m_Axes)
    {
        json axisObj;
        if (entry.kind == AxisEntry::Kind::KeyPair)
        {
            axisObj["kind"] = "KeyPair";
            axisObj["positive"] = Key::ToName(static_cast<Key::Code>(entry.keyPair.Positive.Code));
            axisObj["negative"] = Key::ToName(static_cast<Key::Code>(entry.keyPair.Negative.Code));
        }
        else
        {
            axisObj["kind"] = "MouseAxis";
            axisObj["mouseAxis"] = MouseAxisName(entry.mouseAxis);
        }
        axesObj[name] = std::move(axisObj);
    }
    root["axes"] = std::move(axesObj);

    std::ofstream file(path);
    if (!file.is_open())
    {
        LOG_ERROR("InputActionMap: failed to open '{}' for writing", path);
        return false;
    }

    file << root.dump(2);
    return file.good();
}

bool InputActionMap::LoadFromFile(const std::string &path)
{
    using json = nlohmann::json;

    std::ifstream file(path);
    if (!file.is_open())
        return false;

    json root;
    try
    {
        root = json::parse(file);
    }
    catch (const json::parse_error &e)
    {
        LOG_ERROR("InputActionMap: JSON parse error in '{}': {}", path, e.what());
        return false;
    }

    // Parse into temporaries so we don't corrupt state on partial failure.
    std::unordered_map<std::string, std::vector<InputSource>> newActions;
    std::unordered_map<std::string, AxisEntry> newAxes;

    // Parse actions
    if (root.contains("actions") && root["actions"].is_object())
    {
        for (auto &[name, arr] : root["actions"].items())
        {
            if (!arr.is_array())
                continue;

            std::vector<InputSource> sources;
            for (auto &srcObj : arr)
            {
                if (!srcObj.is_object())
                    continue;

                auto type = ParseSourceType(srcObj.value("type", "Key"));
                auto codeName = srcObj.value("code", "");
                uint16_t code = SourceCodeFromName(type, codeName);

                if (type == InputSource::Type::Key && code == Key::InvalidCode)
                {
                    LOG_WARN("InputActionMap: unknown key '{}' in action '{}'", codeName, name);
                    continue;
                }
                if (type == InputSource::Type::MouseButton && code == Mouse::InvalidCode)
                {
                    LOG_WARN("InputActionMap: unknown mouse button '{}' in action '{}'", codeName, name);
                    continue;
                }

                InputSource src;
                src.SourceType = type;
                src.Code = code;
                src.DeviceIndex = srcObj.value("device", 0);
                sources.push_back(src);
            }

            if (!sources.empty())
                newActions[name] = std::move(sources);
        }
    }

    // Parse axes
    if (root.contains("axes") && root["axes"].is_object())
    {
        for (auto &[name, axisObj] : root["axes"].items())
        {
            if (!axisObj.is_object())
                continue;

            auto kindStr = axisObj.value("kind", "");

            if (kindStr == "KeyPair")
            {
                auto posName = axisObj.value("positive", "");
                auto negName = axisObj.value("negative", "");
                auto posCode = Key::FromName(posName);
                auto negCode = Key::FromName(negName);

                if (posCode == Key::InvalidCode || negCode == Key::InvalidCode)
                {
                    LOG_WARN("InputActionMap: invalid KeyPair axis '{}' (positive='{}', negative='{}')",
                             name, posName, negName);
                    continue;
                }

                AxisEntry entry{};
                entry.kind = AxisEntry::Kind::KeyPair;
                entry.keyPair.Positive = InputSource::FromKey(posCode);
                entry.keyPair.Negative = InputSource::FromKey(negCode);
                newAxes[name] = entry;
            }
            else if (kindStr == "MouseAxis")
            {
                AxisEntry entry{};
                entry.kind = AxisEntry::Kind::MouseAxis;
                entry.mouseAxis = ParseMouseAxis(axisObj.value("mouseAxis", "X"));
                newAxes[name] = entry;
            }
            else
            {
                LOG_WARN("InputActionMap: unknown axis kind '{}' for '{}'", kindStr, name);
            }
        }
    }

    // Commit
    m_Actions = std::move(newActions);
    m_Axes = std::move(newAxes);

    LOG_INFO("InputActionMap: loaded {} actions, {} axes from '{}'",
             m_Actions.size(), m_Axes.size(), path);
    return true;
}

// --- Queries ---

bool InputActionMap::IsActionDown(const std::string &name) const
{
    auto it = m_Actions.find(name);
    if (it == m_Actions.end())
        return false;

    for (const auto &src : it->second)
    {
        if (IsSourceDown(src))
            return true;
    }
    return false;
}

bool InputActionMap::WasActionPressedThisFrame(const std::string &name) const
{
    auto it = m_Actions.find(name);
    if (it == m_Actions.end())
        return false;

    for (const auto &src : it->second)
    {
        if (WasSourcePressedThisFrame(src))
            return true;
    }
    return false;
}

bool InputActionMap::WasActionReleasedThisFrame(const std::string &name) const
{
    auto it = m_Actions.find(name);
    if (it == m_Actions.end())
        return false;

    for (const auto &src : it->second)
    {
        if (WasSourceReleasedThisFrame(src))
            return true;
    }
    return false;
}

float InputActionMap::GetAxis(const std::string &name) const
{
    auto it = m_Axes.find(name);
    if (it == m_Axes.end())
        return 0.0f;

    const AxisEntry &entry = it->second;

    if (entry.kind == AxisEntry::Kind::KeyPair)
    {
        float value = 0.0f;
        if (IsSourceDown(entry.keyPair.Positive))
            value += 1.0f;
        if (IsSourceDown(entry.keyPair.Negative))
            value -= 1.0f;
        return value;
    }

    // MouseAxis
    switch (entry.mouseAxis)
    {
    case MouseAxis::X:
    {
        auto [dx, dy] = Input::GetMouseDelta();
        return dx;
    }
    case MouseAxis::Y:
    {
        auto [dx, dy] = Input::GetMouseDelta();
        return dy;
    }
    case MouseAxis::ScrollY:
        return Input::GetScrollDelta();
    }

    return 0.0f;
}

// --- Source helpers ---

bool InputActionMap::IsSourceDown(const InputSource &source)
{
    switch (source.SourceType)
    {
    case InputSource::Type::Key:
        return Input::IsKeyDown(static_cast<Key::Code>(source.Code));
    case InputSource::Type::MouseButton:
        return Input::IsMouseButtonDown(static_cast<Mouse::Code>(source.Code));
    default:
        return false;
    }
}

bool InputActionMap::WasSourcePressedThisFrame(const InputSource &source)
{
    switch (source.SourceType)
    {
    case InputSource::Type::Key:
        return Input::WasKeyPressedThisFrame(static_cast<Key::Code>(source.Code));
    case InputSource::Type::MouseButton:
        return Input::WasMouseButtonPressedThisFrame(static_cast<Mouse::Code>(source.Code));
    default:
        return false;
    }
}

bool InputActionMap::WasSourceReleasedThisFrame(const InputSource &source)
{
    switch (source.SourceType)
    {
    case InputSource::Type::Key:
        return Input::WasKeyReleasedThisFrame(static_cast<Key::Code>(source.Code));
    case InputSource::Type::MouseButton:
        return Input::WasMouseButtonReleasedThisFrame(static_cast<Mouse::Code>(source.Code));
    default:
        return false;
    }
}
