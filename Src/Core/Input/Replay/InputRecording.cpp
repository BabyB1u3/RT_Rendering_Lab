#include "Core/Input/Replay/InputRecording.h"

#include <cmath>
#include <cstring>
#include <type_traits>

#include "Core/Resource/IO/PhysicalIO.h"

namespace
{
constexpr uint32_t kInputRecordingMagic = 0x52504E49; // INPR
constexpr uint32_t kInputRecordingVersion = 1;
constexpr float kTimingTolerance = 0.0001f;

bool IsPressed(float value)
{
    return value > 0.5f;
}

template <typename T> void AppendPod(std::vector<uint8_t>& bytes, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);

    const auto* begin = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), begin, begin + sizeof(T));
}

template <typename T, std::size_t N> void AppendArray(std::vector<uint8_t>& bytes, const std::array<T, N>& values)
{
    static_assert(std::is_trivially_copyable_v<T>);

    const auto* begin = reinterpret_cast<const uint8_t*>(values.data());
    bytes.insert(bytes.end(), begin, begin + sizeof(T) * values.size());
}

template <typename T> bool ReadPod(const std::vector<uint8_t>& bytes, std::size_t& offset, T& value)
{
    static_assert(std::is_trivially_copyable_v<T>);

    if (offset + sizeof(T) > bytes.size())
        return false;

    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

template <typename T, std::size_t N>
bool ReadArray(const std::vector<uint8_t>& bytes, std::size_t& offset, std::array<T, N>& values)
{
    static_assert(std::is_trivially_copyable_v<T>);

    const std::size_t byteCount = sizeof(T) * values.size();
    if (offset + byteCount > bytes.size())
        return false;

    std::memcpy(values.data(), bytes.data() + offset, byteCount);
    offset += byteCount;
    return true;
}

void CaptureKeyboardState(const InputDeviceManager& manager, RecordedKeyboardState& state)
{
    const auto* device = manager.GetDevice(InputDevice::Type::Keyboard);
    state.Present = (device != nullptr);
    state.Connected = device ? device->IsConnected() : false;
    state.Keys.fill(0);

    if (!device)
        return;

    for (uint16_t key = 0; key < KeyboardDevice::KEY_STATE_SIZE; ++key)
        state.Keys[key] = IsPressed(device->GetInput(key).X) ? 1u : 0u;
}

void CaptureMouseState(const InputDeviceManager& manager, RecordedMouseState& state)
{
    const auto* device = manager.GetDevice(InputDevice::Type::Mouse);
    state.Present = (device != nullptr);
    state.Connected = device ? device->IsConnected() : false;
    state.Buttons.fill(0);
    state.X = 0.0f;
    state.Y = 0.0f;
    state.ScrollDelta = 0.0f;

    if (!device)
        return;

    for (uint16_t button = 0; button < MouseDevice::BUTTON_COUNT; ++button)
        state.Buttons[button] = IsPressed(device->GetInput(button).X) ? 1u : 0u;

    state.X = device->GetAxis(MouseAxisId::PositionX).X;
    state.Y = device->GetAxis(MouseAxisId::PositionY).X;
    state.ScrollDelta = device->GetAxis(MouseAxisId::ScrollY).X;
}

RecordedGamepadState CaptureGamepadState(const InputDevice& device)
{
    RecordedGamepadState state;
    state.DeviceIndex = device.GetDeviceIndex();
    state.Connected = device.IsConnected();
    state.Buttons.fill(0);
    state.Axes.fill(0.0f);

    for (uint16_t button = 0; button < GamepadButton::Count; ++button)
        state.Buttons[button] = IsPressed(device.GetInput(button).X) ? 1u : 0u;

    for (uint16_t axis = 0; axis < GamepadAxis::Count; ++axis)
        state.Axes[axis] = device.GetAxis(axis).X;

    return state;
}
} // namespace

InputRecorder::~InputRecorder()
{
    Detach();
}

void InputRecorder::BeginRecording()
{
    m_Recording.Clear();
    m_NextFrameNumber = 0;
    m_IsRecording = true;
}

void InputRecorder::EndRecording()
{
    m_IsRecording = false;
}

void InputRecorder::Clear()
{
    m_Recording.Clear();
    m_NextFrameNumber = 0;
}

void InputRecorder::CaptureFrame(const InputDeviceManager& manager, uint64_t frameNumber, float dt)
{
    InputFrame frame;
    frame.FrameNumber = frameNumber;
    frame.DeltaTime = dt;

    CaptureKeyboardState(manager, frame.Keyboard);
    CaptureMouseState(manager, frame.Mouse);

    for (const auto* device : manager.GetDevices(InputDevice::Type::Gamepad))
    {
        if (!device)
            continue;

        frame.Gamepads.push_back(CaptureGamepadState(*device));
    }

    m_Recording.Frames.push_back(std::move(frame));
}

void InputRecorder::SetRecording(InputRecording recording)
{
    m_Recording = std::move(recording);
    m_NextFrameNumber = m_Recording.Frames.empty() ? 0 : (m_Recording.Frames.back().FrameNumber + 1);
    m_IsRecording = false;
}

bool InputRecorder::SaveToFile(const std::filesystem::path& path) const
{
    std::vector<uint8_t> bytes;
    bytes.reserve(32 + m_Recording.Frames.size() * 1024);

    AppendPod(bytes, kInputRecordingMagic);
    AppendPod(bytes, kInputRecordingVersion);

    const uint32_t frameCount = static_cast<uint32_t>(m_Recording.Frames.size());
    AppendPod(bytes, frameCount);

    for (const auto& frame : m_Recording.Frames)
    {
        AppendPod(bytes, frame.FrameNumber);
        AppendPod(bytes, frame.DeltaTime);

        const uint8_t keyboardPresent = frame.Keyboard.Present ? 1u : 0u;
        const uint8_t keyboardConnected = frame.Keyboard.Connected ? 1u : 0u;
        AppendPod(bytes, keyboardPresent);
        AppendPod(bytes, keyboardConnected);
        AppendArray(bytes, frame.Keyboard.Keys);

        const uint8_t mousePresent = frame.Mouse.Present ? 1u : 0u;
        const uint8_t mouseConnected = frame.Mouse.Connected ? 1u : 0u;
        AppendPod(bytes, mousePresent);
        AppendPod(bytes, mouseConnected);
        AppendArray(bytes, frame.Mouse.Buttons);
        AppendPod(bytes, frame.Mouse.X);
        AppendPod(bytes, frame.Mouse.Y);
        AppendPod(bytes, frame.Mouse.ScrollDelta);

        const uint32_t gamepadCount = static_cast<uint32_t>(frame.Gamepads.size());
        AppendPod(bytes, gamepadCount);

        for (const auto& gamepad : frame.Gamepads)
        {
            AppendPod(bytes, gamepad.DeviceIndex);
            const uint8_t connected = gamepad.Connected ? 1u : 0u;
            AppendPod(bytes, connected);
            AppendArray(bytes, gamepad.Buttons);
            AppendArray(bytes, gamepad.Axes);
        }
    }

    return Resource::WriteBinaryFile(path, bytes);
}

bool InputRecorder::LoadFromFile(const std::filesystem::path& path)
{
    const auto bytes = Resource::ReadBinaryFile(path);
    if (!bytes)
        return false;

    std::size_t offset = 0;
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t frameCount = 0;

    if (!ReadPod(*bytes, offset, magic) || magic != kInputRecordingMagic || !ReadPod(*bytes, offset, version) ||
        version != kInputRecordingVersion || !ReadPod(*bytes, offset, frameCount))
    {
        return false;
    }

    InputRecording recording;
    recording.Frames.reserve(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        InputFrame frame;

        uint8_t keyboardPresent = 0;
        uint8_t keyboardConnected = 0;
        uint8_t mousePresent = 0;
        uint8_t mouseConnected = 0;
        uint32_t gamepadCount = 0;

        if (!ReadPod(*bytes, offset, frame.FrameNumber) || !ReadPod(*bytes, offset, frame.DeltaTime) ||
            !ReadPod(*bytes, offset, keyboardPresent) || !ReadPod(*bytes, offset, keyboardConnected) ||
            !ReadArray(*bytes, offset, frame.Keyboard.Keys) || !ReadPod(*bytes, offset, mousePresent) ||
            !ReadPod(*bytes, offset, mouseConnected) || !ReadArray(*bytes, offset, frame.Mouse.Buttons) ||
            !ReadPod(*bytes, offset, frame.Mouse.X) || !ReadPod(*bytes, offset, frame.Mouse.Y) ||
            !ReadPod(*bytes, offset, frame.Mouse.ScrollDelta) || !ReadPod(*bytes, offset, gamepadCount))
        {
            return false;
        }

        frame.Keyboard.Present = (keyboardPresent != 0);
        frame.Keyboard.Connected = (keyboardConnected != 0);
        frame.Mouse.Present = (mousePresent != 0);
        frame.Mouse.Connected = (mouseConnected != 0);
        frame.Gamepads.reserve(gamepadCount);

        for (uint32_t gamepadIndex = 0; gamepadIndex < gamepadCount; ++gamepadIndex)
        {
            RecordedGamepadState gamepad;
            uint8_t connected = 0;

            if (!ReadPod(*bytes, offset, gamepad.DeviceIndex) || !ReadPod(*bytes, offset, connected) ||
                !ReadArray(*bytes, offset, gamepad.Buttons) || !ReadArray(*bytes, offset, gamepad.Axes))
            {
                return false;
            }

            gamepad.Connected = (connected != 0);
            frame.Gamepads.push_back(std::move(gamepad));
        }

        recording.Frames.push_back(std::move(frame));
    }

    if (offset != bytes->size())
        return false;

    SetRecording(std::move(recording));
    return true;
}

void InputRecorder::Attach(InputDeviceManager& manager)
{
    if (m_Manager == &manager)
        return;

    Detach();
    m_Manager = &manager;
    m_Manager->AddObserver(this);
}

void InputRecorder::Detach()
{
    if (!m_Manager)
        return;

    m_Manager->RemoveObserver(this);
    m_Manager = nullptr;
}

void InputRecorder::OnAfterPollAll(const InputDeviceManager& manager, float dt)
{
    if (!m_IsRecording)
        return;

    CaptureFrame(manager, m_NextFrameNumber, dt);
    ++m_NextFrameNumber;
}

InputReplaySession::~InputReplaySession()
{
    Detach();
}

void InputReplaySession::SetRecording(InputRecording recording)
{
    m_Recording = std::move(recording);
    Reset();
}

void InputReplaySession::Clear()
{
    m_Recording.Clear();
    Reset();
}

void InputReplaySession::Reset()
{
    m_FrameBeforePrevious = nullptr;
    m_PreviousFrame = nullptr;
    m_CurrentFrame = nullptr;
    m_NextFrameIndex = 0;
    m_IsFinished = m_Recording.Frames.empty();
    m_HasTimingMismatch = false;
}

void InputReplaySession::Attach(InputDeviceManager& manager)
{
    if (m_Manager == &manager)
        return;

    Detach();
    m_Manager = &manager;
    m_Manager->AddObserver(this);
}

void InputReplaySession::Detach()
{
    if (!m_Manager)
        return;

    m_Manager->RemoveObserver(this);
    m_Manager = nullptr;
}

const RecordedKeyboardState* InputReplaySession::GetKeyboardState() const
{
    return (m_CurrentFrame && m_CurrentFrame->Keyboard.Present) ? &m_CurrentFrame->Keyboard : nullptr;
}

const RecordedKeyboardState* InputReplaySession::GetPreviousKeyboardState() const
{
    return (m_PreviousFrame && m_PreviousFrame->Keyboard.Present) ? &m_PreviousFrame->Keyboard : nullptr;
}

const RecordedMouseState* InputReplaySession::GetMouseState() const
{
    return (m_CurrentFrame && m_CurrentFrame->Mouse.Present) ? &m_CurrentFrame->Mouse : nullptr;
}

const RecordedMouseState* InputReplaySession::GetPreviousMouseState() const
{
    return (m_PreviousFrame && m_PreviousFrame->Mouse.Present) ? &m_PreviousFrame->Mouse : nullptr;
}

const RecordedMouseState* InputReplaySession::GetMouseStateBeforePrevious() const
{
    return (m_FrameBeforePrevious && m_FrameBeforePrevious->Mouse.Present) ? &m_FrameBeforePrevious->Mouse : nullptr;
}

const RecordedGamepadState* InputReplaySession::GetGamepadState(uint8_t deviceIndex) const
{
    return FindGamepadState(m_CurrentFrame, deviceIndex);
}

const RecordedGamepadState* InputReplaySession::GetPreviousGamepadState(uint8_t deviceIndex) const
{
    return FindGamepadState(m_PreviousFrame, deviceIndex);
}

void InputReplaySession::OnBeforePollAll(InputDeviceManager&, float dt)
{
    m_FrameBeforePrevious = m_PreviousFrame;
    m_PreviousFrame = m_CurrentFrame;

    if (m_NextFrameIndex >= m_Recording.Frames.size())
    {
        m_CurrentFrame = nullptr;
        m_IsFinished = true;
        return;
    }

    m_CurrentFrame = &m_Recording.Frames[m_NextFrameIndex];
    if (std::fabs(m_CurrentFrame->DeltaTime - dt) > kTimingTolerance)
        m_HasTimingMismatch = true;

    ++m_NextFrameIndex;
    m_IsFinished = (m_NextFrameIndex >= m_Recording.Frames.size());
}

const RecordedGamepadState* InputReplaySession::FindGamepadState(const InputFrame* frame, uint8_t deviceIndex)
{
    if (!frame)
        return nullptr;

    for (const auto& gamepad : frame->Gamepads)
    {
        if (gamepad.DeviceIndex == deviceIndex)
            return &gamepad;
    }

    return nullptr;
}
