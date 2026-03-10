#pragma once

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    virtual void Resize(unsigned int width, unsigned int height) = 0;
};