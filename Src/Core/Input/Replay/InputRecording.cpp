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
    state.isPresent = (device != nullptr);
    state.isConnected = device ? device->IsConnected() : false;
    state.keys.fill(0);

    if (!device)
        return;

    for (uint16_t key = 0; key < KeyboardDevice::k_KeyStateSize; ++key)
        state.keys[key] = IsPressed(device->GetInput(key).x) ? 1u : 0u;
}

void CaptureMouseState(const InputDeviceManager& manager, RecordedMouseState& state)
{
    const auto* device = manager.GetDevice(InputDevice::Type::Mouse);
    state.isPresent = (device != nullptr);
    state.isConnected = device ? device->IsConnected() : false;
    state.buttons.fill(0);
    state.x = 0.0f;
    state.y = 0.0f;
    state.scrollDelta = 0.0f;

    if (!device)
        return;

    for (uint16_t button = 0; button < MouseDevice::k_ButtonCount; ++button)
        state.buttons[button] = IsPressed(device->GetInput(button).x) ? 1u : 0u;

    state.x = device->GetAxis(MouseAxis::PositionX).x;
    state.y = device->GetAxis(MouseAxis::PositionY).x;
    state.scrollDelta = device->GetAxis(MouseAxis::ScrollY).x;
}

RecordedGamepadState CaptureGamepadState(const InputDevice& device)
{
    RecordedGamepadState state;
    state.deviceIndex = device.GetDeviceIndex();
    state.isConnected = device.IsConnected();
    state.buttons.fill(0);
    state.axes.fill(0.0f);

    for (uint16_t button = 0; button < GamepadButton::Count; ++button)
        state.buttons[button] = IsPressed(device.GetInput(button).x) ? 1u : 0u;

    for (uint16_t axis = 0; axis < GamepadAxis::Count; ++axis)
        state.axes[axis] = device.GetAxis(axis).x;

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
    frame.frameNumber = frameNumber;
    frame.deltaTime = dt;

    CaptureKeyboardState(manager, frame.keyboard);
    CaptureMouseState(manager, frame.mouse);

    for (const auto* device : manager.GetDevices(InputDevice::Type::Gamepad))
    {
        if (!device)
            continue;

        frame.gamepads.push_back(CaptureGamepadState(*device));
    }

    m_Recording.frames.push_back(std::move(frame));
}

void InputRecorder::SetRecording(InputRecording recording)
{
    m_Recording = std::move(recording);
    m_NextFrameNumber = m_Recording.frames.empty() ? 0 : (m_Recording.frames.back().frameNumber + 1);
    m_IsRecording = false;
}

bool InputRecorder::SaveToFile(const std::filesystem::path& path) const
{
    std::vector<uint8_t> bytes;
    bytes.reserve(32 + m_Recording.frames.size() * 1024);

    AppendPod(bytes, kInputRecordingMagic);
    AppendPod(bytes, kInputRecordingVersion);

    const uint32_t frameCount = static_cast<uint32_t>(m_Recording.frames.size());
    AppendPod(bytes, frameCount);

    for (const auto& frame : m_Recording.frames)
    {
        AppendPod(bytes, frame.frameNumber);
        AppendPod(bytes, frame.deltaTime);

        const uint8_t keyboardPresent = frame.keyboard.isPresent ? 1u : 0u;
        const uint8_t keyboardConnected = frame.keyboard.isConnected ? 1u : 0u;
        AppendPod(bytes, keyboardPresent);
        AppendPod(bytes, keyboardConnected);
        AppendArray(bytes, frame.keyboard.keys);

        const uint8_t mousePresent = frame.mouse.isPresent ? 1u : 0u;
        const uint8_t mouseConnected = frame.mouse.isConnected ? 1u : 0u;
        AppendPod(bytes, mousePresent);
        AppendPod(bytes, mouseConnected);
        AppendArray(bytes, frame.mouse.buttons);
        AppendPod(bytes, frame.mouse.x);
        AppendPod(bytes, frame.mouse.y);
        AppendPod(bytes, frame.mouse.scrollDelta);

        const uint32_t gamepadCount = static_cast<uint32_t>(frame.gamepads.size());
        AppendPod(bytes, gamepadCount);

        for (const auto& gamepad : frame.gamepads)
        {
            AppendPod(bytes, gamepad.deviceIndex);
            const uint8_t connected = gamepad.isConnected ? 1u : 0u;
            AppendPod(bytes, connected);
            AppendArray(bytes, gamepad.buttons);
            AppendArray(bytes, gamepad.axes);
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
    recording.frames.reserve(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        InputFrame frame;

        uint8_t keyboardPresent = 0;
        uint8_t keyboardConnected = 0;
        uint8_t mousePresent = 0;
        uint8_t mouseConnected = 0;
        uint32_t gamepadCount = 0;

        if (!ReadPod(*bytes, offset, frame.frameNumber) || !ReadPod(*bytes, offset, frame.deltaTime) ||
            !ReadPod(*bytes, offset, keyboardPresent) || !ReadPod(*bytes, offset, keyboardConnected) ||
            !ReadArray(*bytes, offset, frame.keyboard.keys) || !ReadPod(*bytes, offset, mousePresent) ||
            !ReadPod(*bytes, offset, mouseConnected) || !ReadArray(*bytes, offset, frame.mouse.buttons) ||
            !ReadPod(*bytes, offset, frame.mouse.x) || !ReadPod(*bytes, offset, frame.mouse.y) ||
            !ReadPod(*bytes, offset, frame.mouse.scrollDelta) || !ReadPod(*bytes, offset, gamepadCount))
        {
            return false;
        }

        frame.keyboard.isPresent = (keyboardPresent != 0);
        frame.keyboard.isConnected = (keyboardConnected != 0);
        frame.mouse.isPresent = (mousePresent != 0);
        frame.mouse.isConnected = (mouseConnected != 0);
        frame.gamepads.reserve(gamepadCount);

        for (uint32_t gamepadIndex = 0; gamepadIndex < gamepadCount; ++gamepadIndex)
        {
            RecordedGamepadState gamepad;
            uint8_t connected = 0;

            if (!ReadPod(*bytes, offset, gamepad.deviceIndex) || !ReadPod(*bytes, offset, connected) ||
                !ReadArray(*bytes, offset, gamepad.buttons) || !ReadArray(*bytes, offset, gamepad.axes))
            {
                return false;
            }

            gamepad.isConnected = (connected != 0);
            frame.gamepads.push_back(std::move(gamepad));
        }

        recording.frames.push_back(std::move(frame));
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
    m_IsFinished = m_Recording.frames.empty();
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
    return (m_CurrentFrame && m_CurrentFrame->keyboard.isPresent) ? &m_CurrentFrame->keyboard : nullptr;
}

const RecordedKeyboardState* InputReplaySession::GetPreviousKeyboardState() const
{
    return (m_PreviousFrame && m_PreviousFrame->keyboard.isPresent) ? &m_PreviousFrame->keyboard : nullptr;
}

const RecordedMouseState* InputReplaySession::GetMouseState() const
{
    return (m_CurrentFrame && m_CurrentFrame->mouse.isPresent) ? &m_CurrentFrame->mouse : nullptr;
}

const RecordedMouseState* InputReplaySession::GetPreviousMouseState() const
{
    return (m_PreviousFrame && m_PreviousFrame->mouse.isPresent) ? &m_PreviousFrame->mouse : nullptr;
}

const RecordedMouseState* InputReplaySession::GetMouseStateBeforePrevious() const
{
    return (m_FrameBeforePrevious && m_FrameBeforePrevious->mouse.isPresent) ? &m_FrameBeforePrevious->mouse : nullptr;
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

    if (m_NextFrameIndex >= m_Recording.frames.size())
    {
        m_CurrentFrame = nullptr;
        m_IsFinished = true;
        return;
    }

    m_CurrentFrame = &m_Recording.frames[m_NextFrameIndex];
    if (std::fabs(m_CurrentFrame->deltaTime - dt) > kTimingTolerance)
        m_HasTimingMismatch = true;

    ++m_NextFrameIndex;
    m_IsFinished = (m_NextFrameIndex >= m_Recording.frames.size());
}

const RecordedGamepadState* InputReplaySession::FindGamepadState(const InputFrame* frame, uint8_t deviceIndex)
{
    if (!frame)
        return nullptr;

    for (const auto& gamepad : frame->gamepads)
    {
        if (gamepad.deviceIndex == deviceIndex)
            return &gamepad;
    }

    return nullptr;
}
