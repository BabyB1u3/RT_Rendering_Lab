#pragma once

/// @file IUniformBuffer.h
/// @brief Abstract interface for GPU uniform/constant buffers.

#include <cstdint>

class IUniformBuffer
{
public:
    virtual ~IUniformBuffer() = default;

    virtual void SetData(const void *data, uint32_t size, uint32_t offset = 0) = 0;
    virtual uint32_t GetSize() const = 0;
};
