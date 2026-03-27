#pragma once

/// @file InputContextStack.h
/// @brief Priority-sorted stack of InputActionMap contexts with consumption logic.
///
/// Allows multiple InputActionMap instances to coexist with well-defined priority.
/// Higher-priority contexts are processed first; a consuming context blocks all
/// lower-priority contexts from receiving Update() calls or query results.

#include <string>
#include <vector>

class InputActionMap;

/// A single entry in the context stack.
struct InputContext
{
    std::string Name;
    InputActionMap *ActionMap; ///< Non-owning. Lifetime managed by the layer/demo.
    int Priority;              ///< Higher = processed first.
    bool ConsumesInput;        ///< If true AND active, lower-priority contexts are blocked.
    bool Active = true;        ///< Can be temporarily disabled without removing.
};

/// Coordinates multiple InputActionMap instances by priority.
class InputContextStack
{
public:
    /// Add a context. If a context with the same name already exists, it is replaced.
    void Push(const std::string &name, InputActionMap *map,
              int priority = 0, bool consumesInput = false);

    /// Remove a context by name. No-op if the name is not found.
    void Pop(const std::string &name);

    /// Enable or disable a context without removing it.
    void SetActive(const std::string &name, bool active);

    /// Per-frame update: iterates contexts from highest to lowest priority,
    /// calls Update(dt) on each active context's ActionMap.
    /// If an active context has ConsumesInput=true, lower-priority contexts are skipped.
    void Update(float dt);

    // --- Cross-context queries (highest priority wins) ---

    /// Returns the state from the highest-priority active, reachable context that defines the action.
    bool IsActionDown(const std::string &action) const;

    /// Returns the trigger result from the highest-priority active, reachable context that defines the action.
    bool WasActionTriggeredThisFrame(const std::string &action) const;

    /// Returns the axis value from the highest-priority active, reachable context that defines it.
    float GetAxis(const std::string &axis) const;

    // --- Utility ---

    size_t Size() const { return m_Contexts.size(); }
    bool HasContext(const std::string &name) const;

private:
    void SortByPriority();

    std::vector<InputContext> m_Contexts; ///< Sorted by priority descending.
};
