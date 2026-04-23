#pragma once

/// @file VulkanBuffer.h
/// @brief Backend-private Vulkan buffer wrapper backed by VMA.

#include "Core/Diagnostics/Assert/Assert.h"
#include "Render/RHI/Backends/Vulkan/Common/VulkanCommon.h"
#include "Render/RHI/RHIResources.h"

class VulkanBuffer final : public Buffer
{
public:
    VulkanBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation, const BufferDesc& desc)
        : m_Allocator(allocator), m_Buffer(buffer), m_Allocation(allocation), m_Desc(desc)
    {
    }

    ~VulkanBuffer() override
    {
        if (m_Allocator != nullptr && m_Buffer != VK_NULL_HANDLE)
            vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
    }

    const BufferDesc& GetDesc() const override { return m_Desc; }
    VkBuffer GetVkBuffer() const { return m_Buffer; }
    VmaAllocation GetVmaAllocation() const { return m_Allocation; }
    BufferState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(BufferState state) { m_CurrentState = state; }

private:
    VmaAllocator m_Allocator = nullptr;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = nullptr;
    BufferDesc m_Desc;
    BufferState m_CurrentState = BufferState::Undefined;
};

inline VulkanBuffer& GetVulkanBuffer(Buffer* buffer)
{
    auto* vulkanBuffer = dynamic_cast<VulkanBuffer*>(buffer);
    RTRLAB_ASSERT_MSG(vulkanBuffer != nullptr, "Buffer is not owned by the Vulkan backend.");
    return *vulkanBuffer;
}

inline VkBuffer GetVkBufferFromBuffer(Buffer* buffer)
{
    return GetVulkanBuffer(buffer).GetVkBuffer();
}
