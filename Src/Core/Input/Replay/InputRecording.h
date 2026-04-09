#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "Core/Input/Code/GamepadCode.h"
#include "Core/Input/Device/InputDeviceManager.h"
#include "Core/Input/Device/KeyboardDevice.h"
#include "Core/Input/Device/MouseDevice.h"

struct RecordedKeyboardState
{
    bool Present = false;
    bool Connected = false;
    std::array<uint8_t, KeyboardDevice::KEY_STATE_SIZE> Keys{};
};

struct RecordedMouseState
{
    bool Present = false;
    bool Connected = false;
    std::array<uint8_t, MouseDevice::BUTTON_COUNT> Buttons{};
    float X = 0.0f;
    float Y = 0.0f;
    float ScrollDelta = 0.0f;
};

struct RecordedGamepadState
{
    uint8_t DeviceIndex = 0;
    bool Connected = false;
    std::array<uint8_t, GamepadButton::Count> Buttons{};
    std::array<float, GamepadAxis::Count> Axes{};
};

struct InputFrame
{
    uint64_t FrameNumber = 0;
    float DeltaTime = 0.0f;
    RecordedKeyboardState Keyboard;
    RecordedMouseState Mouse;
    std::vector<RecordedGamepadState> Gamepads;
};

struct InputRecording
{
    std::vector<InputFrame> Frames;

    void Clear() { Frames.clear(); }
    bool Empty() const { return Frames.empty(); }
    std::size_t GetFrameCount() const { return Frames.size(); }
};

class InputRecorder final : public InputDeviceManagerObserver
{
public:
    ~InputRecorder() override;

    void BeginRecording();
    void EndRecording();
    bool IsRecording() const { return m_IsRecording; }

    void Clear();
    void CaptureFrame(const InputDeviceManager &manager, uint64_t frameNumber, float dt);

    const InputRecording &GetRecording() const { return m_Recording; }
    void SetRecording(InputRecording recording);

    bool SaveToFile(const std::filesystem::path &path) const;
    bool LoadFromFile(const std::filesystem::path &path);

    void Attach(InputDeviceManager &manager);
    void Detach();

    void OnAfterPollAll(const InputDeviceManager &manager, float dt) override;

private:
    InputRecording m_Recording;
    InputDeviceManager *m_Manager = nullptr; // Non-owning.
    uint64_t m_NextFrameNumber = 0;
    bool m_IsRecording = false;
};

class InputReplaySession final : public InputDeviceManagerObserver
{
public:
    ~InputReplaySession() override;

    void SetRecording(InputRecording recording);
    void Clear();
    void Reset();

    void Attach(InputDeviceManager &manager);
    void Detach();

    bool Empty() const { return m_Recording.Empty(); }
    bool IsFinished() const { return m_IsFinished; }
    bool HasTimingMismatch() const { return m_HasTimingMismatch; }
    std::size_t GetNextFrameIndex() const { return m_NextFrameIndex; }

    const InputFrame *GetCurrentFrame() const { return m_CurrentFrame; }
    const InputFrame *GetPreviousFrame() const { return m_PreviousFrame; }
    const InputFrame *GetFrameBeforePrevious() const { return m_FrameBeforePrevious; }

    const RecordedKeyboardState *GetKeyboardState() const;
    const RecordedKeyboardState *GetPreviousKeyboardState() const;

    const RecordedMouseState *GetMouseState() const;
    const RecordedMouseState *GetPreviousMouseState() const;
    const RecordedMouseState *GetMouseStateBeforePrevious() const;

    const RecordedGamepadState *GetGamepadState(uint8_t deviceIndex) const;
    const RecordedGamepadState *GetPreviousGamepadState(uint8_t deviceIndex) const;

    void OnBeforePollAll(InputDeviceManager &, float dt) override;

private:
    static const RecordedGamepadState *FindGamepadState(const InputFrame *frame, uint8_t deviceIndex);

private:
    InputRecording m_Recording;
    InputDeviceManager *m_Manager = nullptr; // Non-owning.
    const InputFrame *m_FrameBeforePrevious = nullptr;
    const InputFrame *m_PreviousFrame = nullptr;
    const InputFrame *m_CurrentFrame = nullptr;
    std::size_t m_NextFrameIndex = 0;
    bool m_IsFinished = true;
    bool m_HasTimingMismatch = false;
};
