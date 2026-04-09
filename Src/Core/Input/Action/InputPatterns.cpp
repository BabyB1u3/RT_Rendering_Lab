#include "Core/Input/Action/InputPatterns.h"

void ComboTracker::SetCombo(const ComboBinding &combo)
{
    m_Combo = combo;
    Reset();
}

bool ComboTracker::Update(float dt)
{
    if (m_Combo.Steps.empty())
        return false;

    if (m_CurrentStep > 0)
    {
        m_StepTimer += dt;

        const auto &step = m_Combo.Steps[m_CurrentStep];
        if (m_StepTimer > step.MaxDelay)
            Reset();
    }

    const auto &currentStep = m_Combo.Steps[m_CurrentStep];
    if (!InputSourceState::WasPressedThisFrame(currentStep.Source))
        return false;

    ++m_CurrentStep;
    m_StepTimer = 0.0f;

    if (m_CurrentStep < m_Combo.Steps.size())
        return false;

    Reset();
    return true;
}

void ComboTracker::Reset()
{
    m_CurrentStep = 0;
    m_StepTimer = 0.0f;
}
