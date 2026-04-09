#pragma once

/// @file IUniformBuffer.h
/// @brief Abstract interface for GPU uniform/constant buffers.

#include <cstdint>

class IUniformBuffer
{
public:
    virtual ~IUniformBuffer() = default;

    /// Update the buffer contents seen by future draw calls.
    ///
    /// The buffer object itself is just a mutable byte store. If the same
    /// IUniformBuffer is rebound and updated many times during one pass, shader
    /// backends must still honor IShader::BindUniformBuffer()'s per-draw
    /// stability contract.
    virtual void SetData(const void *data, uint32_t size, uint32_t offset = 0) = 0;
    virtual uint32_t GetSize() const = 0;
};
