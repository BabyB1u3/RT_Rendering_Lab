#include <algorithm>

#include "Render/RHI/RHICommandList.h"

namespace
{
// ResourceStateTracker is intentionally conservative at v1: without pass-level
// usage metadata yet, barriers are emitted with an all-stages mask. TRANSITIONAL(M4):
// once pass/resource usage tracking exists, srcStage will come from the last known
// producer stage and dstStage from the next consumer stage instead of this fallback.
constexpr ShaderStage k_BarrierStageMask = ShaderStage::All;
} // namespace

void ResourceStateTracker::Transition(Texture* texture, TextureState newState)
{
    if (texture == nullptr)
        return;

    const auto currentStateIt = m_TextureStates.find(texture);
    const TextureState currentState =
        currentStateIt == m_TextureStates.end() ? TextureState::Undefined : currentStateIt->second;

    if (currentState == newState)
        return;

    for (auto& pending : m_PendingTextureTransitions)
    {
        if (pending.m_Texture == texture)
        {
            pending.m_NewState = newState;
            m_TextureStates[texture] = newState;
            return;
        }
    }

    m_PendingTextureTransitions.push_back({texture, currentState, newState});
    m_TextureStates[texture] = newState;
}

void ResourceStateTracker::Transition(Buffer* buffer, BufferState newState)
{
    if (buffer == nullptr)
        return;

    const auto currentStateIt = m_BufferStates.find(buffer);
    const BufferState currentState =
        currentStateIt == m_BufferStates.end() ? BufferState::Undefined : currentStateIt->second;

    if (currentState == newState)
        return;

    for (auto& pending : m_PendingBufferTransitions)
    {
        if (pending.m_Buffer == buffer)
        {
            pending.m_NewState = newState;
            m_BufferStates[buffer] = newState;
            return;
        }
    }

    m_PendingBufferTransitions.push_back({buffer, currentState, newState});
    m_BufferStates[buffer] = newState;
}

void ResourceStateTracker::Forget(Texture* texture)
{
    if (texture == nullptr)
        return;

    m_TextureStates.erase(texture);
    m_PendingTextureTransitions.erase(std::remove_if(m_PendingTextureTransitions.begin(),
                                                     m_PendingTextureTransitions.end(),
                                                     [texture](const PendingTextureTransition& pending)
                                                     { return pending.m_Texture == texture; }),
                                      m_PendingTextureTransitions.end());
}

void ResourceStateTracker::Forget(Buffer* buffer)
{
    if (buffer == nullptr)
        return;

    m_BufferStates.erase(buffer);
    m_PendingBufferTransitions.erase(std::remove_if(m_PendingBufferTransitions.begin(),
                                                    m_PendingBufferTransitions.end(),
                                                    [buffer](const PendingBufferTransition& pending)
                                                    { return pending.m_Buffer == buffer; }),
                                     m_PendingBufferTransitions.end());
}

void ResourceStateTracker::FlushBarriers(CommandList* commandList)
{
    if (commandList == nullptr)
    {
        m_PendingTextureTransitions.clear();
        m_PendingBufferTransitions.clear();
        return;
    }

    for (const auto& pending : m_PendingTextureTransitions)
    {
        commandList->TextureBarrier(
            pending.m_Texture, pending.m_OldState, pending.m_NewState, k_BarrierStageMask, k_BarrierStageMask);
    }

    for (const auto& pending : m_PendingBufferTransitions)
    {
        commandList->BufferBarrier(
            pending.m_Buffer, pending.m_OldState, pending.m_NewState, k_BarrierStageMask, k_BarrierStageMask);
    }

    m_PendingTextureTransitions.clear();
    m_PendingBufferTransitions.clear();
}

void ResourceStateTracker::Reset()
{
    m_TextureStates.clear();
    m_BufferStates.clear();
    m_PendingTextureTransitions.clear();
    m_PendingBufferTransitions.clear();
}
