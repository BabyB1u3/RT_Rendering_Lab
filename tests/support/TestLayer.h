#pragma once

#include <string>
#include "core/Layer.h"

class TestLayer : public Layer
{
public:
    explicit TestLayer(const std::string &name)
        : Layer(name)
    {
        ++s_LiveCount;
    }

    ~TestLayer() override
    {
        ++s_DestroyedCount;
        --s_LiveCount;
    }

    static void ResetCounters()
    {
        s_LiveCount = 0;
        s_DestroyedCount = 0;
    }

    static int LiveCount() { return s_LiveCount; }
    static int DestroyedCount() { return s_DestroyedCount; }

private:
    inline static int s_LiveCount = 0;
    inline static int s_DestroyedCount = 0;
};