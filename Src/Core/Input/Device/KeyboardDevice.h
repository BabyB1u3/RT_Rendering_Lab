#pragma once

#include <array>

#include "Core/Input/Device/InputDevice.h"

struct GLFWwindow;

class KeyboardDevice final : public InputDevice
{
public:
    static constexpr int k_KeyStateSize = 512;

    explicit KeyboardDevice(GLFWwindow* window = nullptr);

    Type GetType() const override { return Type::Keyboard; }
    void Poll() override;

    InputValue GetInput(uint16_t code) const override;
    InputValue GetPreviousInput(uint16_t code) const override;
    void Reset() override;

    void SetWindow(GLFWwindow* window) { m_Window = window; }
    void ApplyState(const std::array<bool, k_KeyStateSize>& keys);

private:
    GLFWwindow* m_Window = nullptr; // Non-owning. Lifetime is managed by Window/Application.
    std::array<bool, k_KeyStateSize> m_CurrentKeys{};
    std::array<bool, k_KeyStateSize> m_PreviousKeys{};
};
