#pragma once

#include <array>

#include "core/input/InputDevice.h"

struct GLFWwindow;

namespace MouseAxisId
{
    using Code = uint16_t;

    enum : Code
    {
        PositionX = 0,
        PositionY,
        DeltaX,
        DeltaY,
        ScrollY
    };
}

class MouseDevice final : public InputDevice
{
public:
    static constexpr int BUTTON_COUNT = 8;

    explicit MouseDevice(GLFWwindow *window = nullptr);

    Type GetType() const override { return Type::Mouse; }
    void Poll() override;

    InputValue GetInput(uint16_t code) const override;
    InputValue GetPreviousInput(uint16_t code) const override;
    InputValue GetAxis(uint16_t axisId) const override;
    InputValue GetPreviousAxis(uint16_t axisId) const override;
    void Reset() override;

    void SetWindow(GLFWwindow *window) { m_Window = window; }
    void AccumulateScroll(float yOffset);
    void ApplyState(const std::array<bool, BUTTON_COUNT> &buttons, float x, float y);

private:
    GLFWwindow *m_Window = nullptr;

    std::array<bool, BUTTON_COUNT> m_CurrentButtons{};
    std::array<bool, BUTTON_COUNT> m_PreviousButtons{};

    float m_MouseX = 0.0f;
    float m_MouseY = 0.0f;
    float m_LastMouseX = 0.0f;
    float m_LastMouseY = 0.0f;
    bool m_FirstMouseSample = true;

    float m_ScrollAccumulator = 0.0f;
    float m_ScrollThisFrame = 0.0f;
    float m_PreviousScrollThisFrame = 0.0f;
};
