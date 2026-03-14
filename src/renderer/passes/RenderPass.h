#pragma once

struct RenderContext;

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    virtual void Resize(unsigned int width, unsigned int height) = 0;
    virtual void Execute(const RenderContext& ctx) = 0;
};