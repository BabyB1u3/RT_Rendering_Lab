#pragma once

/// @file GLUniformBuffer.h
/// @brief OpenGL implementation of IUniformBuffer using DSA.

#include <cstdint>

#include "graphics/interfaces/IUniformBuffer.h"

class GLUniformBuffer final : public IUniformBuffer
{
public:
    explicit GLUniformBuffer(uint32_t size);
    ~GLUniformBuffer() override;

    GLUniformBuffer(const GLUniformBuffer &) = delete;
    GLUniformBuffer &operator=(const GLUniformBuffer &) = delete;

    GLUniformBuffer(GLUniformBuffer &&other) noexcept;
    GLUniformBuffer &operator=(GLUniformBuffer &&other) noexcept;

    void SetData(const void *data, uint32_t size, uint32_t offset = 0) override;
    uint32_t GetSize() const override { return m_Size; }

    uint32_t GetRendererID() const { return m_RendererID; }

private:
    uint32_t m_RendererID = 0;
    uint32_t m_Size = 0;
};
