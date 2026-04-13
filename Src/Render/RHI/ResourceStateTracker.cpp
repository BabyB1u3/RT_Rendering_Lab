#include "Render/RHI/RHICommandList.h"

namespace
{
    // ResourceStateTracker is intentionally conservative at v1: without pass-level
    // usage metadata yet, barriers are emitted with an all-stages mask.
    constexpr ShaderStage kBarrierStageMask = ShaderStage::All;
}

void ResourceStateTracker::transition(Texture *texture, TextureState newState)
{
    if (texture == nullptr)
        return;

    const auto currentStateIt = m_TextureStates.find(texture);
    const TextureState currentState = currentStateIt == m_TextureStates.end()
        ? TextureState::Undefined
        : currentStateIt->second;

    if (currentState == newState)
        return;

    for (auto &pending : m_PendingTextureTransitions)
    {
        if (pending.texture == texture)
        {
            pending.newState = newState;
            m_TextureStates[texture] = newState;
            return;
        }
    }

    m_PendingTextureTransitions.push_back({texture, currentState, newState});
    m_TextureStates[texture] = newState;
}

void ResourceStateTracker::transition(Buffer *buffer, BufferState newState)
{
    if (buffer == nullptr)
        return;

    const auto currentStateIt = m_BufferStates.find(buffer);
    const BufferState currentState = currentStateIt == m_BufferStates.end()
        ? BufferState::Undefined
        : currentStateIt->second;

    if (currentState == newState)
        return;

    for (auto &pending : m_PendingBufferTransitions)
    {
        if (pending.buffer == buffer)
        {
            pending.newState = newState;
            m_BufferStates[buffer] = newState;
            return;
        }
    }

    m_PendingBufferTransitions.push_back({buffer, currentState, newState});
    m_BufferStates[buffer] = newState;
}

void ResourceStateTracker::flushBarriers(CommandList *commandList)
{
    if (commandList == nullptr)
    {
        m_PendingTextureTransitions.clear();
        m_PendingBufferTransitions.clear();
        return;
    }

    for (const auto &pending : m_PendingTextureTransitions)
    {
        commandList->textureBarrier(
            pending.texture,
            pending.oldState,
            pending.newState,
            kBarrierStageMask,
            kBarrierStageMask);
    }

    for (const auto &pending : m_PendingBufferTransitions)
    {
        commandList->bufferBarrier(
            pending.buffer,
            pending.oldState,
            pending.newState,
            kBarrierStageMask,
            kBarrierStageMask);
    }

    m_PendingTextureTransitions.clear();
    m_PendingBufferTransitions.clear();
}

void ResourceStateTracker::reset()
{
    m_TextureStates.clear();
    m_BufferStates.clear();
    m_PendingTextureTransitions.clear();
    m_PendingBufferTransitions.clear();
}
