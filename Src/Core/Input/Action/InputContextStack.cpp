#include "Core/Input/Action/InputContextStack.h"
#include "Core/Input/Action/InputAction.h"

#include <algorithm>

// --- Lifecycle ---

void InputContextStack::Push(const std::string& name, InputActionMap* map, int priority, bool consumesInput)
{
    // Replace existing context with the same name.
    Pop(name);

    InputContext ctx;
    ctx.name = name;
    ctx.actionMap = map;
    ctx.priority = priority;
    ctx.consumesInput = consumesInput;
    ctx.isActive = true;

    m_Contexts.push_back(std::move(ctx));
    SortByPriority();
}

void InputContextStack::Pop(const std::string& name)
{
    auto it = std::find_if(m_Contexts.begin(), m_Contexts.end(), [&](const InputContext& c) { return c.name == name; });
    if (it != m_Contexts.end())
    {
        if (it->actionMap != nullptr)
            it->actionMap->ResetRuntimeState();

        m_Contexts.erase(it);
    }
}

void InputContextStack::SetActive(const std::string& name, bool active)
{
    auto it = std::find_if(m_Contexts.begin(), m_Contexts.end(), [&](const InputContext& c) { return c.name == name; });
    if (it != m_Contexts.end())
    {
        if (it->isActive && !active && it->actionMap != nullptr)
            it->actionMap->ResetRuntimeState();

        it->isActive = active;
    }
}

// --- Per-frame update ---

void InputContextStack::Update(float dt)
{
    std::vector<InputSource> blockedSources;

    for (auto& ctx : m_Contexts)
    {
        if (!ctx.isActive || ctx.actionMap == nullptr)
            continue;

        ctx.actionMap->Update(dt, blockedSources);
        ctx.actionMap->AppendBlockingChordSources(blockedSources);

        if (ctx.consumesInput)
            break;
    }
}

// --- Cross-context queries ---

bool InputContextStack::IsActionDown(const std::string& action) const
{
    std::vector<InputSource> blockedSources;

    for (const auto& ctx : m_Contexts)
    {
        if (!ctx.isActive || ctx.actionMap == nullptr)
            continue;

        if (ctx.actionMap->HasActionAvailable(action, blockedSources))
            return ctx.actionMap->IsActionDown(action, blockedSources);

        ctx.actionMap->AppendBlockingChordSources(blockedSources);

        if (ctx.consumesInput)
            break;
    }

    return false;
}

bool InputContextStack::WasActionTriggeredThisFrame(const std::string& action) const
{
    std::vector<InputSource> blockedSources;

    for (const auto& ctx : m_Contexts)
    {
        if (!ctx.isActive || ctx.actionMap == nullptr)
            continue;

        if (ctx.actionMap->HasActionAvailable(action, blockedSources))
            return ctx.actionMap->WasActionTriggeredThisFrame(action, blockedSources);

        ctx.actionMap->AppendBlockingChordSources(blockedSources);

        if (ctx.consumesInput)
            break;
    }

    return false;
}

float InputContextStack::GetAxis(const std::string& axis) const
{
    for (const auto& ctx : m_Contexts)
    {
        if (!ctx.isActive || ctx.actionMap == nullptr)
            continue;

        if (ctx.actionMap->HasAxis(axis))
            return ctx.actionMap->GetAxis(axis);

        if (ctx.consumesInput)
            break;
    }

    return 0.0f;
}

// --- Utility ---

bool InputContextStack::HasContext(const std::string& name) const
{
    return std::any_of(m_Contexts.begin(), m_Contexts.end(), [&](const InputContext& c) { return c.name == name; });
}

void InputContextStack::SortByPriority()
{
    std::stable_sort(m_Contexts.begin(),
                     m_Contexts.end(),
                     [](const InputContext& a, const InputContext& b) { return a.priority > b.priority; });
}
