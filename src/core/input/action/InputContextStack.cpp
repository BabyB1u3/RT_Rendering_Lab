#include "core/input/action/InputContextStack.h"
#include "core/input/action/InputAction.h"

#include <algorithm>

namespace
{
    template <typename Predicate>
    const InputContext *FindFirstReachableContext(const std::vector<InputContext> &contexts,
                                                  Predicate &&predicate)
    {
        for (const auto &ctx : contexts)
        {
            if (!ctx.Active || ctx.ActionMap == nullptr)
                continue;

            if (predicate(ctx))
                return &ctx;

            if (ctx.ConsumesInput)
                break;
        }

        return nullptr;
    }
}

// --- Lifecycle ---

void InputContextStack::Push(const std::string &name, InputActionMap *map,
                             int priority, bool consumesInput)
{
    // Replace existing context with the same name.
    Pop(name);

    InputContext ctx;
    ctx.Name = name;
    ctx.ActionMap = map;
    ctx.Priority = priority;
    ctx.ConsumesInput = consumesInput;
    ctx.Active = true;

    m_Contexts.push_back(std::move(ctx));
    SortByPriority();
}

void InputContextStack::Pop(const std::string &name)
{
    auto it = std::find_if(m_Contexts.begin(), m_Contexts.end(),
                           [&](const InputContext &c)
                           { return c.Name == name; });
    if (it != m_Contexts.end())
    {
        if (it->ActionMap != nullptr)
            it->ActionMap->ResetRuntimeState();

        m_Contexts.erase(it);
    }
}

void InputContextStack::SetActive(const std::string &name, bool active)
{
    auto it = std::find_if(m_Contexts.begin(), m_Contexts.end(),
                           [&](const InputContext &c)
                           { return c.Name == name; });
    if (it != m_Contexts.end())
    {
        if (it->Active && !active && it->ActionMap != nullptr)
            it->ActionMap->ResetRuntimeState();

        it->Active = active;
    }
}

// --- Per-frame update ---

void InputContextStack::Update(float dt)
{
    for (auto &ctx : m_Contexts)
    {
        if (!ctx.Active || ctx.ActionMap == nullptr)
            continue;

        ctx.ActionMap->Update(dt);

        if (ctx.ConsumesInput)
            break;
    }
}

// --- Cross-context queries ---

bool InputContextStack::IsActionDown(const std::string &action) const
{
    const InputContext *ctx = FindFirstReachableContext(
        m_Contexts, [&](const InputContext &candidate)
        { return candidate.ActionMap->HasAction(action); });
    return ctx != nullptr ? ctx->ActionMap->IsActionDown(action) : false;
}

bool InputContextStack::WasActionTriggeredThisFrame(const std::string &action) const
{
    const InputContext *ctx = FindFirstReachableContext(
        m_Contexts, [&](const InputContext &candidate)
        { return candidate.ActionMap->HasAction(action); });
    return ctx != nullptr ? ctx->ActionMap->WasActionTriggeredThisFrame(action) : false;
}

float InputContextStack::GetAxis(const std::string &axis) const
{
    const InputContext *ctx = FindFirstReachableContext(
        m_Contexts, [&](const InputContext &candidate)
        { return candidate.ActionMap->HasAxis(axis); });
    return ctx != nullptr ? ctx->ActionMap->GetAxis(axis) : 0.0f;
}

// --- Utility ---

bool InputContextStack::HasContext(const std::string &name) const
{
    return std::any_of(m_Contexts.begin(), m_Contexts.end(),
                       [&](const InputContext &c)
                       { return c.Name == name; });
}

void InputContextStack::SortByPriority()
{
    std::stable_sort(m_Contexts.begin(), m_Contexts.end(),
                     [](const InputContext &a, const InputContext &b)
                     { return a.Priority > b.Priority; });
}
