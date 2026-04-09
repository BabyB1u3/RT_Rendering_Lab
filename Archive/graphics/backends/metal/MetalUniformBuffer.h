#pragma once

/// @file MetalUniformBuffer.h
/// @brief Metal implementation of IUniformBuffer - wraps a MTLBuffer.

#include <cstdint>
#include <memory>

#include "graphics/interfaces/IUniformBuffer.h"

class MetalUniformBuffer final : public IUniformBuffer
{
public:
    explicit MetalUniformBuffer(uint32_t size);
    ~MetalUniformBuffer() override;

    MetalUniformBuffer(const MetalUniformBuffer &) = delete;
    MetalUniformBuffer &operator=(const MetalUniformBuffer &) = delete;

    void SetData(const void *data, uint32_t size, uint32_t offset = 0) override;
    uint32_t GetSize() const override { return m_Size; }

    void *GetMTLBuffer() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
    uint32_t m_Size = 0;
};
