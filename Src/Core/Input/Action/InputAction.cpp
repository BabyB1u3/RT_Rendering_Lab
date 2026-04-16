#include "Core/Input/Action/InputAction.h"

#include "Core/Input/Device/InputDeviceManager.h"
#include "Core/Input/Input.h"

namespace
{
const std::vector<InputSource>& NoBlockedSources()
{
    static const std::vector<InputSource> sources;
    return sources;
}
} // namespace

// --- Registration ---

void InputActionMap::BindAction(const std::string& name, InputSource source)
{
    m_Actions[name].push_back(source);
}

void InputActionMap::BindChordAction(const std::string& name, std::vector<InputSource> sources)
{
    if (sources.empty())
        return;

    m_ChordActions[name].push_back({std::move(sources)});
}

void InputActionMap::BindChordAction(const std::string& name, std::initializer_list<InputSource> sources)
{
    BindChordAction(name, std::vector<InputSource>(sources));
}

void InputActionMap::BindAction(const std::string& name, Key::Code key)
{
    BindAction(name, InputSource::FromKey(key));
}

void InputActionMap::BindAxis(const std::string& name, Key::Code positive, Key::Code negative)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::KeyPair;
    entry.keyPair.positive = InputSource::FromKey(positive);
    entry.keyPair.negative = InputSource::FromKey(negative);
    m_Axes[name] = entry;
}

void InputActionMap::BindAxis(const std::string& name, MouseAxis mouseAxis)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::MouseAxis;
    entry.mouseAxis = mouseAxis;
    m_Axes[name] = entry;
}

void InputActionMap::BindAxis(const std::string& name, GamepadAxis::Code gamepadAxis, uint8_t deviceIndex)
{
    AxisEntry entry{};
    entry.kind = AxisEntry::Kind::GamepadAxis;
    entry.gamepadAxis = gamepadAxis;
    entry.deviceIndex = deviceIndex;
    m_Axes[name] = entry;
}

void InputActionMap::Unbind(const std::string& name)
{
    m_Actions.erase(name);
    m_ChordActions.erase(name);
    m_Axes.erase(name);
    m_Modifiers.erase(name);
    m_Triggers.erase(name);
    m_TriggerStates.erase(name);
    m_CachedAxisValues.erase(name);
    m_CachedActionStates.erase(name);
}

void InputActionMap::Clear()
{
    m_Actions.clear();
    m_ChordActions.clear();
    m_Axes.clear();
    m_Modifiers.clear();
    m_Triggers.clear();
    m_TriggerStates.clear();
    m_CachedAxisValues.clear();
    m_CachedActionStates.clear();
}

bool InputActionMap::HasAction(const std::string& name) const
{
    return m_Actions.find(name) != m_Actions.end() || m_ChordActions.find(name) != m_ChordActions.end();
}

bool InputActionMap::HasAxis(const std::string& name) const
{
    return m_Axes.find(name) != m_Axes.end();
}

void InputActionMap::AddModifier(const std::string& axisName, Scope<InputModifier> modifier)
{
    m_Modifiers[axisName].push_back(std::move(modifier));
}

void InputActionMap::SetTrigger(const std::string& actionName, Scope<InputTrigger> trigger)
{
    if (trigger)
    {
        m_Triggers[actionName] = std::move(trigger);
        m_TriggerStates.erase(actionName);
    }
    else
    {
        m_Triggers.erase(actionName);
        m_TriggerStates.erase(actionName);
    }
}

void InputActionMap::Update(float dt)
{
    Update(dt, NoBlockedSources());
}

void InputActionMap::Update(float dt, const std::vector<InputSource>& blockedSources)
{
    // Evaluate axis modifiers and cache results.
    m_CachedAxisValues.clear();
    m_CachedActionStates.clear();
    for (const auto& [name, entry] : m_Axes)
    {
        float value = ComputeRawAxis(entry);

        auto modIt = m_Modifiers.find(name);
        if (modIt != m_Modifiers.end())
        {
            for (const auto& mod : modIt->second)
                value = mod->Apply(value, dt);
        }

        m_CachedAxisValues[name] = value;
    }

    for (const auto& [name, sources] : m_Actions)
    {
        (void)sources;
        m_CachedActionStates[name] = EvaluateActionState(name, blockedSources);
    }

    for (const auto& [name, chords] : m_ChordActions)
    {
        (void)chords;
        if (!m_CachedActionStates.contains(name))
            m_CachedActionStates[name] = EvaluateActionState(name, blockedSources);
    }

    // Advance trigger state machines.
    for (auto& [name, trigger] : m_Triggers)
    {
        const ActionState state = m_CachedActionStates.contains(name) ? m_CachedActionStates.at(name)
                                                                      : EvaluateActionState(name, blockedSources);
        if (!state.hasAvailableBinding)
        {
            m_TriggerStates[name] = TriggerState::None;
            continue;
        }

        m_TriggerStates[name] = trigger->Evaluate(state.isDown, state.wasPressed, state.wasReleased, dt);
    }
}

// --- Queries ---

bool InputActionMap::IsActionDown(const std::string& name) const
{
    auto it = m_CachedActionStates.find(name);
    if (it != m_CachedActionStates.end())
        return it->second.hasAvailableBinding && it->second.isDown;

    return IsActionDown(name, NoBlockedSources());
}

bool InputActionMap::IsActionDown(const std::string& name, const std::vector<InputSource>& blockedSources) const
{
    const ActionState state = EvaluateActionState(name, blockedSources);
    return state.hasAvailableBinding && state.isDown;
}

bool InputActionMap::WasActionPressedThisFrame(const std::string& name) const
{
    auto it = m_CachedActionStates.find(name);
    if (it != m_CachedActionStates.end())
        return it->second.hasAvailableBinding && it->second.wasPressed;

    return WasActionPressedThisFrame(name, NoBlockedSources());
}

bool InputActionMap::WasActionPressedThisFrame(const std::string& name,
                                               const std::vector<InputSource>& blockedSources) const
{
    const ActionState state = EvaluateActionState(name, blockedSources);
    return state.hasAvailableBinding && state.wasPressed;
}

bool InputActionMap::WasActionReleasedThisFrame(const std::string& name) const
{
    auto it = m_CachedActionStates.find(name);
    if (it != m_CachedActionStates.end())
        return it->second.hasAvailableBinding && it->second.wasReleased;

    return WasActionReleasedThisFrame(name, NoBlockedSources());
}

bool InputActionMap::WasActionReleasedThisFrame(const std::string& name,
                                                const std::vector<InputSource>& blockedSources) const
{
    const ActionState state = EvaluateActionState(name, blockedSources);
    return state.hasAvailableBinding && state.wasReleased;
}

float InputActionMap::GetAxis(const std::string& name) const
{
    // If Update() was called this frame, return the cached (modifier-applied) value.
    auto cachedIt = m_CachedAxisValues.find(name);
    if (cachedIt != m_CachedAxisValues.end())
        return cachedIt->second;

    // Fall back to raw computation (no modifiers, or Update() not called).
    auto it = m_Axes.find(name);
    if (it == m_Axes.end())
        return 0.0f;

    return ComputeRawAxis(it->second);
}

bool InputActionMap::WasActionTriggeredThisFrame(const std::string& name) const
{
    auto triggerIt = m_TriggerStates.find(name);
    if (triggerIt != m_TriggerStates.end())
        return triggerIt->second == TriggerState::Triggered;

    if (m_Triggers.find(name) != m_Triggers.end())
        return false;

    auto actionIt = m_CachedActionStates.find(name);
    if (actionIt != m_CachedActionStates.end())
        return actionIt->second.hasAvailableBinding && actionIt->second.wasPressed;

    return WasActionTriggeredThisFrame(name, NoBlockedSources());
}

bool InputActionMap::WasActionTriggeredThisFrame(const std::string& name,
                                                 const std::vector<InputSource>& blockedSources) const
{
    if (!HasActionAvailable(name, blockedSources))
        return false;

    auto it = m_TriggerStates.find(name);
    if (it != m_TriggerStates.end())
        return it->second == TriggerState::Triggered;

    if (m_Triggers.find(name) != m_Triggers.end())
        return false;

    // No trigger set - fall back to default pressed behavior.
    return WasActionPressedThisFrame(name, blockedSources);
}

TriggerState InputActionMap::GetActionTriggerState(const std::string& name) const
{
    auto it = m_TriggerStates.find(name);
    if (it != m_TriggerStates.end())
        return it->second;

    if (m_Triggers.find(name) != m_Triggers.end())
        return TriggerState::None;

    // No trigger set - emulate pressed trigger.
    return WasActionPressedThisFrame(name) ? TriggerState::Triggered : TriggerState::None;
}

void InputActionMap::ResetRuntimeState()
{
    m_CachedAxisValues.clear();
    m_CachedActionStates.clear();
    m_TriggerStates.clear();

    for (auto& [name, trigger] : m_Triggers)
    {
        if (trigger)
            trigger->Reset();
    }
}

bool InputActionMap::HasActionAvailable(const std::string& name, const std::vector<InputSource>& blockedSources) const
{
    return EvaluateActionState(name, blockedSources).hasAvailableBinding;
}

void InputActionMap::AppendBlockingChordSources(std::vector<InputSource>& blockedSources) const
{
    for (const auto& [name, chords] : m_ChordActions)
    {
        (void)name;
        for (const auto& chord : chords)
        {
            if (!IsChordBlocking(chord))
                continue;

            for (const auto& source : chord.sources)
                InputSourceState::AppendUnique(blockedSources, source);
        }
    }
}

float InputActionMap::ComputeRawAxis(const AxisEntry& entry) const
{
    if (entry.kind == AxisEntry::Kind::KeyPair)
    {
        float value = 0.0f;
        if (InputSourceState::IsDown(entry.keyPair.positive))
            value += 1.0f;
        if (InputSourceState::IsDown(entry.keyPair.negative))
            value -= 1.0f;
        return value;
    }

    if (entry.kind == AxisEntry::Kind::MouseAxis)
    {
        switch (entry.mouseAxis)
        {
            case MouseAxis::X:
                return Input::GetMouseDelta().first;
            case MouseAxis::Y:
                return Input::GetMouseDelta().second;
            case MouseAxis::ScrollY:
                return Input::GetScrollDelta();
        }
    }

    const auto* manager = Input::TryGetDeviceManager();
    if (!manager)
        return 0.0f;

    const auto* device = manager->GetDevice(InputDevice::Type::Gamepad, entry.deviceIndex);
    return device ? device->GetAxis(entry.gamepadAxis).x : 0.0f;
}

InputActionMap::ActionState InputActionMap::EvaluateActionState(const std::string& name,
                                                                const std::vector<InputSource>& blockedSources) const
{
    ActionState state{};

    auto actionIt = m_Actions.find(name);
    if (actionIt != m_Actions.end())
    {
        for (const auto& source : actionIt->second)
        {
            if (InputSourceState::IsBlocked(source, blockedSources))
                continue;

            state.hasAvailableBinding = true;
            state.isDown = state.isDown || InputSourceState::IsDown(source);
            state.wasPressed = state.wasPressed || InputSourceState::WasPressedThisFrame(source);
            state.wasReleased = state.wasReleased || InputSourceState::WasReleasedThisFrame(source);
        }
    }

    auto chordIt = m_ChordActions.find(name);
    if (chordIt != m_ChordActions.end())
    {
        for (const auto& binding : chordIt->second)
        {
            if (IsChordBlocked(binding, blockedSources))
                continue;

            state.hasAvailableBinding = true;

            const bool down = IsChordDown(binding);
            const bool wasDown = WasChordDown(binding);

            state.isDown = state.isDown || down;
            state.wasPressed = state.wasPressed || (down && !wasDown);
            state.wasReleased = state.wasReleased || (!down && wasDown);
        }
    }

    return state;
}

bool InputActionMap::IsChordDown(const ChordBinding& binding)
{
    if (binding.sources.empty())
        return false;

    for (const auto& source : binding.sources)
    {
        if (!InputSourceState::IsDown(source))
            return false;
    }

    return true;
}

bool InputActionMap::WasChordDown(const ChordBinding& binding)
{
    if (binding.sources.empty())
        return false;

    for (const auto& source : binding.sources)
    {
        if (!InputSourceState::WasDown(source))
            return false;
    }

    return true;
}

bool InputActionMap::IsChordBlocking(const ChordBinding& binding)
{
    for (const auto& source : binding.sources)
    {
        if (InputSourceState::IsDown(source))
            return true;
    }

    return false;
}

bool InputActionMap::IsChordBlocked(const ChordBinding& binding, const std::vector<InputSource>& blockedSources)
{
    for (const auto& source : binding.sources)
    {
        if (InputSourceState::IsBlocked(source, blockedSources))
            return true;
    }

    return false;
}
