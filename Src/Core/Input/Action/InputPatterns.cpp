#include "Core/Input/Action/InputPatterns.h"

void ComboTracker::SetCombo(const ComboBinding& combo)
{
    m_Combo = combo;
    Reset();
}

bool ComboTracker::Update(float dt)
{
    if (m_Combo.steps.empty())
        return false;

    if (m_CurrentStep > 0)
    {
        m_StepTimer += dt;

        const auto& step = m_Combo.steps[m_CurrentStep];
        if (m_StepTimer > step.maxDelay)
            Reset();
    }

    const auto& currentStep = m_Combo.steps[m_CurrentStep];
    if (!InputSourceState::WasPressedThisFrame(currentStep.source))
        return false;

    ++m_CurrentStep;
    m_StepTimer = 0.0f;

    if (m_CurrentStep < m_Combo.steps.size())
        return false;

    Reset();
    return true;
}

void ComboTracker::Reset()
{
    m_CurrentStep = 0;
    m_StepTimer = 0.0f;
}
