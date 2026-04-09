#pragma once

#include <cstddef>
#include <vector>

#include "Core/Input/Action/InputSource.h"

/// A chord requires all listed sources to be active simultaneously.
struct ChordBinding
{
    std::vector<InputSource> Sources;
};

/// A combo requires a sequence of source presses within per-step time limits.
struct ComboBinding
{
    struct Step
    {
        InputSource Source;
        float MaxDelay = 0.0f;
    };

    std::vector<Step> Steps;
};

class ComboTracker
{
public:
    void SetCombo(const ComboBinding &combo);
    bool Update(float dt);
    void Reset();

    std::size_t GetCurrentStep() const { return m_CurrentStep; }
    bool IsIdle() const { return m_CurrentStep == 0; }

private:
    ComboBinding m_Combo;
    std::size_t m_CurrentStep = 0;
    float m_StepTimer = 0.0f;
};
